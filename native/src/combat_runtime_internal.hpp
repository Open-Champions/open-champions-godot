#pragma once
#include "champions/arena_monsters.hpp"
#include "champions/combat_damage.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/combat_stats.hpp"
#include "champions/core_state.hpp"
#include "champions/creature_movement.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace champions::simulation::rta::combat_runtime {
namespace A = arena_monsters;
using Value = A::Value;
inline Value iv(std::int64_t v) { return v; }
inline Raw number(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v))
    return Raw(*p);
  throw Error("integer service result required");
}
inline int byte(std::uint64_t n) {
  return n & 128 ? int(n & 255) - 256 : int(n & 255);
}
struct World::State {
  struct Actor : Object {
    bool player = false;
    bool alive = true;
  };
  struct Overlay;
  Pool pool;
  HandlePool handles;
  SharedRandom shared;
  std::map<Address, std::unique_ptr<Cell>> heads;
  std::map<const Link *, Address> links;
  std::map<Address, std::unique_ptr<Actor>> objects;
  std::vector<std::unique_ptr<Actor>> retired;
  std::shared_ptr<Overlay> overlay;
  creature_hp::Memory mem;
  A::Model model;
  creature_movement::Model movement;
  Services services;
  std::vector<Event> trace;
  std::map<Address, std::pair<Address, Raw>>
      queued_events; // key is external handle, not actor
  Address fixture_next = 0x10000, external_next = 0x90000000;
  int camera_mode = 1;
  bool running = false, failed = false;
  std::vector<std::unique_ptr<Iterator>>
      iterators; // preserve retired prev links
  std::map<Address, std::pair<Raw, Raw>> animations;
  std::array<std::uint8_t, 32> music{};
  explicit State(Services, int);
  ~State();
  Raw u(Address a) const { return Raw(mem.load(a, 4)); }
  std::uint64_t q(Address a) const { return mem.load(a, 8); }
  Raw h(Address a) const { return Raw(mem.load(a, 2)); }
  Raw b(Address a) const { return Raw(mem.load(a, 1)); }
  void w(Address a, unsigned n, std::uint64_t v) { mem.store(a, n, v); }
  void w(Address a, Raw v) { w(a, 4, v); }
  Vec3 vec(Address a) const { return {u(a), u(a + 4), u(a + 8)}; }
  void vec(Address a, const Vec3 &v) {
    for (unsigned k = 0; k < 3; ++k)
      w(a + 4 * k, v[k]);
  }
  Actor &actor(Address a) const;
  Cell &head(Address);
  Address ptr(const Link *) const;
  Address target(Address) const;
  Address alloc_fixture(unsigned);
  Address spawn(const Spawn &, bool, unsigned, int, int);
  void link(Address, Address, Address);
  void clear(Address);
  void relink(Address, Address);
  void remove(Address);
  void grid(Address);
  Address home(Address, bool) const;
  void schedule(Address, unsigned);
  void destroy(Address);
  void erase_later(Address);
  void walk(Address, const std::function<void(Address)> &);
  std::vector<Address> box(const Vec3 &, const Vec3 &, int);
  void run_objects();
  void tick();
  void run(Address);
  void skelight_run(Address);
  void player_run(Address);
  void alive();
  void domain() const;
  void actor_domain(Address) const;
  void note(std::string name, std::vector<std::uint64_t> args = {});
  void install_services();
  Value service(const std::string &, const std::vector<Value> &);
  Value vcall(Address, unsigned, std::vector<Value> = {});
  int enemy(Address, Address);
  Raw record(Address) const;
  Raw skill_rank(Raw, Raw) const;
  Raw draw();
  Raw own(Address);
  void take_damage(Address, Address, Raw, Raw);
  int damage(Address, Address, Raw, Raw, Raw, Raw);
  void process_damage(Address);
  void player_take_damage(Address, Address, Raw, Raw);
  void player_process_damage(Address);
  void player_find_target(Address, Raw);
  void player_melee_hit(Address, Address, Raw, Raw);
  void recalc(Address);
  void state3(Address);
  void reaction(Address);
  int can_see(Address, Address, Raw, Raw, Raw, Raw, Raw, Raw);
  int hit_arc(Address, const Arc &, bool);
  Address cone(Address, Raw, Raw, Raw, Raw);
  Raw ray(const Vec3 &, const Vec3 &, Raw, Raw);
  Raw move(Address, const Vec3 &);
  void knockback(Address, Raw);
  void dead_update(Address, Raw, Raw, Raw);
  void start_anim(Address, Raw, Raw, Raw, Raw = 0);
  Raw advance_anim(Address, Raw);
  void tables();
};
} // namespace champions::simulation::rta::combat_runtime
