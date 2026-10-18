#include "champions/combat_runtime.hpp"
#include "champions/core_state.hpp"
#include "champions/session_combat_runtime_adapter.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace {
unsigned checks = 0;
void check(bool b, const std::string &s) {
  ++checks;
  if (!b)
    throw std::runtime_error(s);
}
template <class F> void refused(F fn, const std::string &s) {
  bool caught = false;
  try {
    fn();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, s);
}
struct Fixture {
  C::World *ptr = nullptr;
  std::map<Raw, std::pair<Raw, Raw>> anim;
  Raw ray = MINUS_ONE, travel = 0;
  bool dialog = false;
  C::Services services() {
    auto s = C::flat_fixture_services();
    s.dialog_is_open = [this] { return dialog; };
    s.jumpgate_travel_state = [this] { return travel; };
    s.raycast = [this](const Vec3 &, const Vec3 &, Raw, Raw) { return ray; };
    s.animation_start = [this](Raw a, Raw clip, Raw, Raw, Raw) {
      anim[a] = {clip, 0};
      ptr->write(a + 0x118, 1, 1);
      ptr->write(a + 0x11a, 2, 8);
    };
    s.animation_advance = [this](Raw a, Raw) {
      auto i = anim.find(a);
      if (i == anim.end())
        return Raw(0);
      auto &v = i->second;
      bool swing = v.first == 0xc1100018 || v.first == 0xc1100020;
      auto t = ++v.second;
      if (t >= (swing ? 30u : 20u))
        ptr->write(a + 0x11a, 2, (ptr->read(a + 0x11a, 2) & ~8u) | 0x80);
      return Raw(swing && t == 10 ? 1 : 0);
    };
    return s;
  }
  C::World world;
  explicit Fixture(int mode = 1) : world(services(), mode) { ptr = &world; }
};
void hosts() {
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    auto a = w.spawn_skelight(s);
    s.position[0] = from_int(40);
    auto b = w.spawn_skelight(s);
    auto ext = w.retain(b);
    w.relink(a + C::TARGET, b);
    w.relink(a + C::LAST_ATTACKER, b);
    auto node = Raw(w.read(a + C::TARGET));
    check(w.read(node + 12) == b, "handle points to pool address");
    refused([&] { w.write(a + C::TARGET, 4, 123); }, "raw handle store");
    refused([&] { w.write(node + 12, 4, 0); }, "raw node store");
    refused([&] { w.relink(a + C::TARGET, 0x123450); }, "foreign target");
    w.destroy(b);
    check(!w.target(ext) && !w.target(a + C::TARGET) &&
              !w.target(a + C::LAST_ATTACKER),
          "destruction nulls references");
    auto c = w.spawn_skelight(s);
    check(c == b && !w.target(ext),
          "same class reuse without handle resurrection");
    w.write(c + 16, 4, from_int(700));
    w.update_grid(c);
    auto list = w.collect_box({from_int(600), 0, from_int(-10)},
                              {from_int(800), from_int(100), from_int(10)}, 16);
    check(list == std::vector<Raw>{c}, "one image grid position");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 0;
    auto p = w.spawn_player(0, s);
    s.position[0] = from_int(30);
    s.team = -1;
    auto m = w.spawn_skelight(s);
    Lcg seed;
    seed.draw();
    seed.draw();
    check(w.read(C::SHARED_SEED) == seed.state, "constructor shared draws");
    check(w.enemy(p, 0) == 0 && w.enemy(m, 0) == 0,
          "null target is not an enemy");
    auto start = seed.state;
    w.damage(p, m, from_int(4), from_int(8), 100000, 8);
    for (unsigned i = 0; i < 7; ++i)
      seed.draw();
    check(w.read(C::SHARED_SEED) == seed.state, "monster seven shared draws");
    check(w.read(C::PLAYERS + 0xac) == from_int(100) && w.read(p + 0xc8) != 0,
          "player accumulates");
    w.damage(m, p, from_int(4), from_int(8), 100000, 1);
    for (unsigned i = 0; i < 8; ++i)
      seed.draw();
    check(w.read(C::SHARED_SEED) == seed.state && start != seed.state,
          "player eight shared draws");
  }
  {
    Fixture f(0);
    auto &w = f.world;
    C::Spawn s;
    s.team = 1;
    auto a = w.spawn_skelight(s);
    s.team = 2;
    s.position[0] = from_int(30);
    s.facing = 0x8000;
    auto b = w.spawn_skelight(s);
    w.tick();
    std::vector<Raw> order;
    for (auto &e : w.events())
      if (e.name == "run")
        order.push_back(Raw(e.args[0]));
    check(order == std::vector<Raw>{b, a}, "last linked first");
    check(w.target(a + C::TARGET) == b && w.target(b + C::TARGET) == a,
          "enemy targets");
    bool pending = false, immediate = false;
    for (unsigned k = 0; k < 65; ++k) {
      w.clear_history();
      w.tick();
      for (auto &e : w.events())
        if (e.name == "damage_result" && e.args[1]) {
          auto v = Raw(e.args[0]);
          if (v == b && w.read(b + 0xc8)) {
            pending = true;
            w.tick();
            check(w.read(b + 0xc8) == 0, "next-run reaction");
          }
          if (v == a && w.read(a + 0xc8) == 0)
            immediate = true;
        }
    }
    check(pending && immediate, "both run-order damage timings");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 1;
    auto a = w.spawn_skelight(s);
    s.team = 2;
    s.position[0] = from_int(30);
    s.facing = 0x8000;
    w.spawn_skelight(s);
    for (unsigned k = 0; k < 60; ++k)
      w.tick();
    bool hit = false;
    for (auto &e : w.events())
      if (e.name == "damage_apply")
        hit = true;
    check(!hit && w.read(a + 0x232, 2) == 6 && !(w.read(a + 0xa8, 8) >> 63),
          "inactive swing never lands");
  }
  {
    Fixture f(0);
    auto &w = f.world;
    C::Spawn s;
    s.team = 0;
    auto p = w.spawn_player(0, s);
    s.team = -1;
    s.position[0] = from_int(30);
    s.hp = ONE;
    auto m = w.spawn_skelight(s);
    w.relink(p + C::TARGET, m);
    auto h = w.retain(m);
    w.take_damage(m, p, 8, from_int(10));
    check(w.read(m + 0x228) == from_int(-9) && w.contains(m),
          "lethal hit immediate hp only");
    w.process_damage(m);
    check((w.read(m + 12) & 9) == 9 && w.target(h) == m &&
              w.read(m + 0x232, 2) == 20,
          "death queues delete with live handles");
    auto list = w.collect_box({0, from_int(-50), from_int(-10)},
                              {from_int(60), from_int(50), from_int(10)}, 16);
    check(std::find(list.begin(), list.end(), m) != list.end(),
          "corpse stays in grid");
    w.run_objects();
    check(!w.contains(m) && !w.target(h) && !w.target(p + C::TARGET),
          "next flush destroys");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 0;
    auto p = w.spawn_player(0, s);
    w.take_damage(p, 0, 8, from_int(200));
    w.process_damage(p);
    check(w.contains(p) && w.read(p + 0x58c) == 4 &&
              (w.read(p + 12) & 9) == 9 && w.read(0x4f8420) == 0,
          "dead player kept and alive list immediate");
    refused([&] { w.delete_object(p); }, "player never deleted");
    refused([&] { w.tick(); }, "party wipe refusal");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    auto p = w.spawn_player(0, s);
    w.take_damage(p, 0, 8, from_int(10));
    Lcg seed(Raw(w.read(C::SHARED_SEED)));
    seed.draw();
    w.process_damage(p);
    check(w.read(C::SHARED_SEED) == seed.state &&
              w.read(C::PLAYERS + 0xac) == from_int(90),
          "surviving player hit stagger draw");
    w.take_damage(p, 0, 0x40008, ONE);
    seed.draw();
    w.process_damage(p);
    check(w.read(C::SHARED_SEED) == seed.state && w.read(p + 0x58c) == 3,
          "strong stance draw and small reaction suppress stagger");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 1;
    auto c = w.spawn_skelight(s);
    s.team = 2;
    s.position[0] = from_int(60);
    auto far = w.spawn_skelight(s);
    check(w.find_enemy_in_cone(c, 0, 0x2000, 0, from_int(48)) == far,
          "cone has no hard far cap");
    check(w.can_see(c, far, 0x8000, 0x2000, 0, 0, from_int(48)) == 0,
          "behind arc rejection");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.creature_flags = 0;
    auto c = w.spawn_skelight(s);
    w.write(c + 0x2e8, 4, from_int(100));
    auto own = w.read(c + 0x44), shared = w.read(C::SHARED_SEED);
    w.apply_knockback(c, 0x3f333333);
    check(w.read(c + 0x44) != own && w.read(C::SHARED_SEED) == shared &&
              w.read(c + 0x2e8) == from_int(98) && w.read(c + 16) > 0,
          "knockback dust own stream");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    auto c = w.spawn_skelight(s);
    auto h = w.retain(c);
    C::SessionAdapter a(w);
    refused([&] { a.ready_tick(1); }, "out of order ready tick");
    a.hit_test_event(h, 1);
    a.ready_tick(0);
    check(w.read(C::FRAME) == 1, "one ready tick");
    w.destroy(c);
    refused([&] { a.hit_test_event(h, 1); }, "stale animation event");
    for (auto name : {"moveWithCollision", "projectiles",
                      "restricted Player_move", "full destructor chain",
                      "findPlayerInCone", "remaining hit/cooldown helpers",
                      "walking", "combos", "weapons", "blocking", "knock-downs",
                      "other classes death/loot", "spells", "PvP", "network"})
      refused([&] { w.refuse(name); }, name);
  }

  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 0;
    auto p = w.spawn_player(0, s);
    s.position[0] = from_int(30);
    auto m = w.spawn_skelight(s);
    w.relink(p + C::TARGET, m);
    w.write(p + 0x58c, 4, 1);
    w.write(m + 12, 4, w.read(m + 12) | 0x8000);
    auto h = w.retain(p);
    C::SessionAdapter adapter(w);
    auto hp = w.read(m + 0x228);
    adapter.hit_test_event(h, 2);
    adapter.ready_tick(0);
    check(w.read(m + 0x228) == hp, "non-hit animation event ignored");
    adapter.hit_test_event(h, 1);
    adapter.ready_tick(1);
    check(w.read(m + 0x228) < hp,
          "mask one drives unarmed hit in player's run");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    auto p = w.spawn_player(0, s);
    s.position[0] = from_int(30);
    auto m = w.spawn_skelight(s);
    refused([&] { w.relink(m + 12, p); },
            "nonzero raw field cannot become handle head");
    w.write(C::PLAYERS + 0x4434, 4, 0x1234);
    refused([&] { w.player_melee_hit(p, m); }, "weapon branch refuses");
    w.write(C::PLAYERS + 0x4434, 4, 0);
    refused([&] { w.player_melee_hit(p, m, 0, ONE); }, "scaled hit refuses");
    f.ray = 0;
    refused([&] { w.player_melee_hit(p, m); }, "blocked line of sight refuses");
    f.ray = MINUS_ONE;
    w.write(p + 0x58c, 4, 8);
    refused([&] { w.take_damage(p, m, 8, ONE); }, "blocking intake refuses");
    w.write(p + 0x58c, 4, 2);
    w.take_damage(p, m, 0x80008, ONE);
    refused([&] { w.process_damage(p); }, "knock-down reaction refuses");
    w.write(m + 0xa8, 8, 0x200);
    refused([&] { w.damage(m, p, ONE, TWO, 100, 8); },
            "blocking damage refuses");
    w.write(m + 0xa8, 8, 0x100000);
    w.relink(m + 0x364, p);
    refused([&] { w.damage(m, p, ONE, TWO, 100, 8); },
            "attached spells refuse");
    refused([&] { w.destroy(m); }, "full destructor refuses");
    w.clear_handle(m + 0x364);
    w.write(0x402791, 1, 1);
    C::SessionAdapter adapter(w);
    refused([&] { adapter.ready_tick(0); }, "network tick refuses");
    check(adapter.failed(), "adapter failure latch");
    w.write(0x402791, 1, 0);
    refused([&] { adapter.ready_tick(0); }, "failed tick cannot replay");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 1;
    auto a = w.spawn_skelight(s);
    s.team = 2;
    s.position[0] = from_int(30);
    auto b = w.spawn_skelight(s);
    w.write(a + 0x48, 4, 0x1234);
    refused([&] { w.enemy(a, b); }, "unknown enemy table refuses");
    refused([&] { w.take_damage(a, b, 8, ONE); },
            "unknown damage table refuses");
  }

  {
    C::World *ptr = nullptr;
    Raw born = 0;
    auto services = C::flat_fixture_services();
    services.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    services.animation_advance = [&](Raw, Raw) {
      if (!born) {
        C::Spawn child;
        child.team = 1;
        born = ptr->spawn_skelight(child);
      }
      return Raw(0);
    };
    C::World w(services, 0);
    ptr = &w;
    C::Spawn s;
    s.team = 1;
    auto first = w.spawn_skelight(s);
    w.tick();
    std::vector<Raw> order;
    for (auto &e : w.events())
      if (e.name == "run")
        order.push_back(Raw(e.args[0]));
    check(born && order == std::vector<Raw>{first},
          "front link during walk waits until next pass");
    w.clear_history();
    w.tick();
    order.clear();
    for (auto &e : w.events())
      if (e.name == "run")
        order.push_back(Raw(e.args[0]));
    check(order == std::vector<Raw>{born, first},
          "new front link runs first next pass");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    auto active = w.spawn_skelight(s), slow = w.spawn_skelight(s),
         slower = w.spawn_skelight(s);
    for (auto a : {active, slow, slower})
      w.write(a + 0x48, 4, C::VT_CREATURE);
    w.run_every(slow, 15);
    w.run_every(slower, 60);
    w.write(C::FRAME, 4, 1);
    w.run_objects();
    std::vector<Raw> order;
    for (auto &e : w.events())
      if (e.name == "run")
        order.push_back(Raw(e.args[0]));
    check(order == std::vector<Raw>{active, slow, slower},
          "active then 15 then 60 list order");
    w.clear_history();
    w.delete_object(active);
    w.delete_object(slow);
    w.run_objects();
    order.clear();
    for (auto &e : w.events())
      if (e.name == "destroy")
        order.push_back(Raw(e.args[0]));
    check(order == std::vector<Raw>{slow, active},
          "delete list flush is last deleted first");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.team = 1;
    auto a = w.spawn_skelight(s);
    s.team = 2;
    s.hp = ONE;
    s.position[0] = from_int(30);
    auto b = w.spawn_skelight(s);
    w.relink(a + C::TARGET, b);
    w.take_damage(b, a, 8, from_int(10));
    w.process_damage(b);
    C::Arc ar;
    ar.attack = 100000;
    check(w.enemy(a, b) == 0 && w.hit_target_in_arc(a, ar) == 1,
          "arc target helper can still hit dying handle without enemy or dead "
          "test");
  }
  {
    Fixture f;
    auto &w = f.world;
    C::Spawn s;
    s.creature_flags = 0;
    auto a = w.spawn_skelight(s);
    w.write(a + 0x1c, 4, 0);
    w.write(a + 0x2e8, 4, from_int(100));
    auto seed = w.read(C::SHARED_SEED);
    refused([&] { w.apply_knockback(a, ONE); },
            "dust radius below one refuses signed remainder by zero");
    check(w.read(C::SHARED_SEED) == seed,
          "failed own-generator dust never consumes shared seed");
  }
  std::cout << checks << " native combat runtime checks passed\n";
}
void vectors(const char *path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot open vectors");
  std::unique_ptr<Fixture> f;
  std::string line, label;
  unsigned n = 0;
  while (std::getline(in, line)) {
    ++n;
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream s(line);
    std::string op;
    s >> op;
    auto num = [&]() {
      std::uint64_t n;
      if (!(s >> n))
        throw std::runtime_error("malformed vector");
      return n;
    };
    auto r = [&]() { return Raw(num()); };
    try {
      if (op == "CASE") {
        s >> label;
        auto mode = int(num());
        f = std::make_unique<Fixture>(mode);
        continue;
      }
      if (!f)
        throw std::runtime_error("case required");
      auto &w = f->world;
      if (op == "S" || op == "P") {
        C::Spawn v;
        unsigned slot = 0;
        if (op == "P")
          slot = r();
        for (auto &x : v.position)
          x = r();
        v.facing = r();
        v.hp = r();
        v.team = s32(r());
        v.creature_flags = num();
        v.armor = s32(r());
        auto expected = r();
        auto got = op == "P" ? w.spawn_player(slot, v) : w.spawn_skelight(v);
        check(got == expected, "spawn address");
      } else if (op == "W") {
        auto a = r();
        auto z = r();
        auto v = num();
        w.write(a, z, v);
      } else if (op == "R") {
        auto a = r();
        auto z = r();
        auto expected = num();
        auto got = w.read(a, z);
        check(got == expected, "memory at " + std::to_string(a) + ": got " +
                                   std::to_string(got) + ", expected " +
                                   std::to_string(expected));
      } else if (op == "LINK") {
        auto a = r(), b = r();
        w.relink(a, b);
      } else if (op == "GRID")
        w.update_grid(r());
      else if (op == "DIALOG")
        f->dialog = r() != 0;
      else if (op == "TRAVEL")
        f->travel = r();
      else if (op == "RAY")
        f->ray = r();
      else if (op == "TICK")
        w.tick();
      else if (op == "RUN")
        w.run_objects();
      else if (op == "TAKE") {
        auto a = r(), b = r(), fl = r(), amt = r();
        w.take_damage(a, b, fl, amt);
      } else if (op == "PROCESS")
        w.process_damage(r());
      else if (op == "DAMAGE") {
        auto v = r(), a = r(), lo = r(), hi = r(), ar = r(), fl = r(),
             want = r();
        check(w.damage(v, a, lo, hi, ar, fl) == int(want), "damage result");
      } else if (op == "SEE") {
        auto a = r(), b = r(), f = r(), arc = r(), mode = r(), near = r(),
             far = r(), lead = r(), want = r();
        check(w.can_see(a, b, f, arc, mode, near, far, lead) == int(want),
              "visibility");
      } else if (op == "CONE") {
        auto a = r(), f = r(), arc = r(), near = r(), far = r(), want = r();
        check(w.find_enemy_in_cone(a, f, arc, near, far) == want,
              "cone result");
      } else if (op == "ENEMY") {
        auto a = r(), b = r(), want = r();
        check(w.enemy(a, b) == int(want), "enemy result");
      } else if (op == "FIND") {
        auto a = r(), reach = r();
        w.player_find_target(a, reach);
      } else if (op == "PICK") {
        auto a = r(), force = r();
        w.pick_target(a, force != 0);
      } else if (op == "MELEE") {
        auto a = r(), b = r(), e = r();
        w.player_melee_hit(a, b, e);
      } else if (op == "ARC") {
        auto c = r(), all = r();
        C::Arc a;
        a.facing = r();
        a.angle = r();
        a.minimum = r();
        a.maximum = r();
        a.attack = r();
        a.flags = r();
        a.near = r();
        a.far = r();
        auto want = r();
        check((all ? w.hit_all_in_arc(c, a) : w.hit_target_in_arc(c, a)) ==
                  int(want),
              "arc result");
      } else if (op == "KNOCK") {
        auto a = r(), scale = r();
        w.apply_knockback(a, scale);
      } else if (op == "DEAD") {
        auto a = r(), ev = r(), hold = r(), size = r();
        w.dead_update(a, ev, hold, size);
      } else if (op == "DELETE")
        w.delete_object(r());
      else if (op == "DESTROY")
        w.destroy(r());
      else if (op == "ORDER") {
        std::string name;
        s >> name;
        unsigned count = r();
        std::vector<Raw> want;
        for (unsigned k = 0; k < count; ++k)
          want.push_back(r());
        std::vector<Raw> got;
        for (auto &e : w.events())
          if (e.name == name)
            got.push_back(Raw(e.args[0]));
        check(got == want, "event order " + name);
      } else if (op == "CLEAR")
        w.clear_history();
      else if (op == "MOVE") {
        auto a = r();
        Vec3 v;
        for (auto &x : v)
          x = r();
        auto want = r();
        check(w.move_creature(a, v) == want, "move bits");
      } else
        throw std::runtime_error("unknown operation " + op);
    } catch (const std::exception &e) {
      throw std::runtime_error(label + " line " + std::to_string(n) + " " + op +
                               ": " + e.what());
    }
  }
  std::cout << checks << " exact memory/result/order comparisons passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2)
      vectors(argv[1]);
    else
      hosts();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
