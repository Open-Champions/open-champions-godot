#include "champions/combat_runtime.hpp"
#include "champions/core_state.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
// Replays the round-2 synthetic fixtures generated from the pinned research
// composition (tools/generate_combat_runtime_2_vectors.py). The fixture below is
// the research's AUTHORED test environment: flat or stepped terrain, the
// PlayerAnim timeline, root motion, resource tokens, attach points at the
// object's position, and model sample points absent.
using namespace champions::simulation::rta;
namespace C = champions::simulation::rta::combat_runtime;
namespace {
std::uint64_t checks = 0;
struct Mismatch : std::logic_error {
  using std::logic_error::logic_error;
};
void check(bool b, const std::string &s) {
  ++checks;
  if (!b)
    throw Mismatch(s);
}
constexpr std::uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL,
                        FNV_PRIME = 0x100000001b3ULL;
struct Fnv {
  std::uint64_t h = FNV_OFFSET;
  void feed(std::uint64_t v, unsigned bytes) {
    for (unsigned k = 0; k < bytes; ++k)
      h = (h ^ ((v >> (8 * k)) & 0xff)) * FNV_PRIME;
  }
};
struct Fixture {
  struct Clip {
    unsigned length = 20;
    std::map<unsigned, Raw> events;
  };
  C::World *world = nullptr;
  std::map<Raw, Clip> clips;
  std::map<C::Address, std::pair<Raw, unsigned>> state;
  std::set<C::Address> players;
  std::map<C::Address, std::array<Raw, 2>> root;
  Raw ray = MINUS_ONE, terrain_z = 0, step_x = 0, step_h = 0;
  bool step = false, planes = false;
  std::array<C::Plane, 4> plane{};
  Raw yaw = 0, skill_stat = 0;
  bool dialog = false;
  Raw travel = 0;
  std::string loot_class;
  void poke(C::Address a, unsigned n, std::uint64_t v) { world->write(a, n, v); }
  std::uint64_t peek(C::Address a, unsigned n) { return world->read(a, n); }
  void start(C::Address actor, Raw clip, Raw flags) {
    auto anim = actor + 0x114;
    state[anim] = {clip, 0};
    if (players.count(actor)) {
      poke(anim + 4, 1, 5);
      poke(anim + 8 + 0x80, 4, clip);
      poke(anim + 8 + 0x80 + 0x1c, 2, flags & 0xffff);
      poke(anim + 6, 2, 8);
    } else {
      poke(anim + 4, 1, 1);
      poke(anim + 6, 2, 8);
    }
  }
  Raw advance(C::Address actor) {
    auto anim = actor + 0x114;
    auto i = state.find(anim);
    if (i == state.end())
      return 0;
    auto c = clips.find(i->second.first);
    Clip clip = c == clips.end() ? Clip{} : c->second;
    auto tick = ++i->second.second;
    auto e = clip.events.find(tick);
    Raw bits = e == clip.events.end() ? 0 : e->second;
    if (tick >= clip.length) {
      auto ended = (Raw(peek(anim + 6, 2)) & ~8u & 0xffff) | 0x80;
      if (players.count(actor)) {
        state.erase(i);
        poke(anim + 4, 1, 4);
        poke(anim + 8 + 0x80, 4, 0);
        poke(anim + 8 + 0x80 + 0x1c, 2, 0);
      }
      poke(anim + 6, 2, ended);
    }
    return bits;
  }
  C::Services services() {
    C::Services s;
    s.dialog_is_open = [this] { return dialog; };
    s.jumpgate_travel_state = [this] { return travel; };
    s.raycast = [this](const Vec3 &, const Vec3 &, Raw, Raw) { return ray; };
    s.slide = [](const Vec3 &p, const Vec3 &d, Raw, Raw) {
      C::Slide out;
      for (unsigned k = 0; k < 3; ++k)
        out.position[k] = add(p[k], d[k]);
      return out;
    };
    s.height_below = [this](const Vec3 &p, Raw) {
      return step && le(step_x, p[0]) ? step_h : terrain_z;
    };
    s.water_test = [](const Vec3 &, Raw) { return Raw(0); };
    s.animation_start = [this](Raw a, Raw clip, Raw flags, Raw, Raw) {
      start(a, clip, flags);
    };
    s.animation_advance = [this](Raw a, Raw) { return advance(a); };
    s.animation_root = [this](C::Address a) {
      auto i = root.find(a + 0x114);
      return i == root.end() ? std::array<Raw, 2>{0, 0} : i->second;
    };
    s.skill_stat = [this](const std::string &, Raw, Raw) { return skill_stat; };
    s.camera_yaw = [this](Raw) { return yaw; };
    s.camera_flag = [](Raw) { return Raw(0); };
    s.attach_point = [](C::Address, Raw, const Vec3 &pos, Raw, Raw) {
      return pos;
    };
    s.sample_point_count = [](Raw) { return Raw(0); };
    s.resource = [](const C::ResourceQuery &q) -> Raw {
      if (q.lump_is_text)
        return 0;
      std::uint64_t sum = 0;
      for (unsigned char ch : q.entry)
        sum += ch;
      return 0xb0000000u | Raw((sum * 131 + q.entry.size()) & 0xffffff);
    };
    s.is_class = [this](C::Address, const std::string &name) {
      return !loot_class.empty() && name == loot_class + "_typeinfo";
    };
    s.loot_generate = [](const C::LootDrop &) {};
    s.spawn_named_item = [](const Vec3 &, Raw) {};
    return s;
  }
};
const std::set<std::string> EVENT_KINDS = {
    "run",         "damage_apply",    "damage_result", "obj_delete",
    "destroy",     "destroy_spell",   "cast",          "player_tick",
    "pool_new",    "light",           "player_giveExp", "loot_generateDrops",
    "loot_spawnNamedItem"};
struct Replay {
  std::unique_ptr<Fixture> fx;
  std::unique_ptr<C::World> world;
  std::string label;
  std::size_t mark = 0;
  bool dead = false, last_refused = false;
  std::string refusal;
  std::map<std::uint32_t, std::uint8_t> shadow; // reference memory so far
  std::vector<std::uint64_t> result;
  bool result_list = false;
  std::map<std::uint32_t, std::uint8_t> memory() {
    std::map<std::uint32_t, std::uint8_t> out;
    for (auto &b : world->memory().bytes())
      if (b.second)
        out[b.first] = b.second;
    return out;
  }
  void run_case(int mode, bool planes, const std::array<C::Plane, 4> &plane) {
    fx = std::make_unique<Fixture>();
    fx->planes = planes;
    fx->plane = plane;
    auto services = fx->services();
    if (planes) {
      auto p = plane;
      services.screen_planes = [p](const Vec3 &) { return p; };
    }
    world.reset();
    world = std::make_unique<C::World>(services, mode, C::Composition::Full);
    fx->world = world.get();
    mark = 0;
    dead = false;
  }
};
Vec3 v3(std::istringstream &s) {
  Vec3 v;
  for (auto &x : v) {
    std::uint64_t n;
    s >> n;
    x = Raw(n);
  }
  return v;
}
void vectors(const char *path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot open vectors");
  Replay r;
  std::string line;
  unsigned long n = 0, cases = 0, ops = 0;
  while (std::getline(in, line)) {
    ++n;
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream s(line);
    std::string op;
    s >> op;
    auto num = [&]() {
      std::uint64_t v;
      if (!(s >> v))
        throw std::runtime_error("malformed vector");
      return v;
    };
    auto raw = [&]() { return Raw(num()); };
    try {
      if (op == "CASE") {
        s >> r.label;
        auto mode = int(num());
        bool planes = num() != 0;
        std::array<C::Plane, 4> plane{};
        if (planes)
          for (auto &p : plane) {
            p.point = v3(s);
            p.normal = v3(s);
          }
        r.run_case(mode, planes, plane);
        r.shadow.clear();
        ++cases;
        continue;
      }
      if (!r.world)
        throw std::runtime_error("case required");
      auto &w = *r.world;
      auto &f = *r.fx;
      // ---- checks after an operation ----------------------------------------
      if (op == "EV") {
        auto count = num();
        std::vector<std::string> want;
        for (std::uint64_t k = 0; k < count; ++k) {
          if (!std::getline(in, line))
            throw std::runtime_error("truncated events");
          ++n;
          want.push_back(line);
        }
        std::vector<std::string> got;
        auto &events = w.events();
        for (std::size_t k = r.mark; k < events.size(); ++k) {
          auto &e = events[k];
          if (!EVENT_KINDS.count(e.name))
            continue;
          std::ostringstream o;
          o << "e " << e.name << ' ' << e.args.size();
          for (auto a : e.args)
            o << ' ' << (a & 0xffffffffu);
          if (!e.text.empty()) {
            auto t = e.text;
            for (auto &ch : t)
              if (ch == ' ')
                ch = '_';
            o << ' ' << t;
          }
          got.push_back(o.str());
        }
        r.mark = events.size();
        if (got != want) {
          std::ostringstream o;
          o << "events differ\n want:";
          for (auto &x : want)
            o << "\n   " << x;
          o << "\n got:";
          for (auto &x : got)
            o << "\n   " << x;
          throw std::runtime_error(o.str());
        }
        checks += want.size() + 1;
        continue;
      }
      if (op == "D") {
        auto count = num();
        auto mem = r.memory();
        for (std::uint64_t k = 0; k < count; ++k) {
          auto a = std::uint32_t(num());
          auto v = std::uint8_t(num());
          auto i = mem.find(a);
          auto got = i == mem.end() ? 0 : i->second;
          if (v)
            r.shadow[a] = v;
          else
            r.shadow.erase(a);
          if (got != v) {
            std::ostringstream o;
            o << "byte " << std::hex << a << ": got " << unsigned(got)
              << ", expected " << unsigned(v);
            throw std::runtime_error(o.str());
          }
          ++checks;
        }
        continue;
      }
      if (op == "M") {
        auto count = num();
        auto digest = num();
        auto mem = r.memory();
        Fnv h;
        for (auto &b : mem) {
          h.feed(b.first, 4);
          h.feed(b.second, 1);
        }
        if (mem.size() != count || h.h != digest) {
          std::ostringstream o;
          o << "memory digest: " << mem.size() << " bytes, expected " << count
            << std::hex;
          unsigned shown = 0;
          for (auto &b : mem) {
            auto i = r.shadow.find(b.first);
            if ((i == r.shadow.end() || i->second != b.second) && shown++ < 40)
              o << "\n  native " << b.first << " = " << unsigned(b.second)
                << " reference "
                << (i == r.shadow.end() ? 0u : unsigned(i->second));
          }
          for (auto &b : r.shadow)
            if (!mem.count(b.first) && shown++ < 40)
              o << "\n  missing " << b.first << " = " << unsigned(b.second);
          throw std::runtime_error(o.str());
        }
        ++checks;
        continue;
      }
      if (op == "HD") {
        auto count = num();
        std::map<C::Address, C::Address> heads;
        for (auto &h : w.handle_words())
          heads[h.first] = h.second;
        for (std::uint64_t k = 0; k < count; ++k) {
          auto a = C::Address(num());
          auto v = C::Address(num());
          auto i = heads.find(a);
          auto got = i == heads.end() ? 0 : i->second;
          if (got != v) {
            std::ostringstream o;
            o << "handle word " << std::hex << a << ": got " << got
              << ", expected " << v;
            throw std::runtime_error(o.str());
          }
          ++checks;
        }
        continue;
      }
      if (op == "H") {
        auto count = num();
        auto digest = num();
        auto heads = w.handle_words();
        Fnv h;
        for (auto &x : heads) {
          h.feed(x.first, 4);
          h.feed(x.second, 4);
        }
        check(heads.size() == count && h.h == digest, "handle digest");
        continue;
      }
      if (op == "NODES") {
        auto count = num();
        auto digest = num();
        auto free = num();
        Fnv h;
        std::uint64_t found = 0;
        auto nodes = w.handle_nodes();
        for (unsigned i = 0; i < nodes.size(); ++i)
          for (unsigned k = 0; k < 5; ++k)
            if (auto word = nodes[i][k]) {
              h.feed(i * 8 + k, 4);
              h.feed(word, 4);
              ++found;
            }
        if (!(found == count && h.h == digest && w.free_handle_node() == free))
          if (auto dump = std::getenv("CR2_DUMP_NODES")) {
            std::ofstream out(dump);
            out << std::hex;
            for (unsigned i = 0; i < nodes.size(); ++i) {
              out << std::dec << i << std::hex;
              for (auto x : nodes[i])
                out << ' ' << x;
              out << '\n';
            }
            out << "free " << w.free_handle_node() << '\n';
          }
        check(found == count && h.h == digest && w.free_handle_node() == free,
              "handle node pool");
        continue;
      }
      if (op == "R" || op == "RL") {
        std::vector<std::uint64_t> want;
        if (op == "R")
          want.push_back(num());
        else {
          auto count = num();
          for (std::uint64_t k = 0; k < count; ++k)
            want.push_back(num());
        }
        std::vector<std::uint64_t> got;
        for (auto v : r.result)
          got.push_back(v & 0xffffffffu);
        check(got == want, "call result");
        continue;
      }
      if (op == "REFUSED") {
        check(r.last_refused, "operation should have refused");
        r.dead = true;
        continue;
      }
      if (r.dead)
        throw std::runtime_error("operation after a refusal");
      if (r.last_refused)
        throw std::runtime_error("unexpected refusal before " + op);
      ++ops;
      r.mark = w.events().size();
      r.result.clear();
      // ---- fixture settings ---------------------------------------------------
      if (op == "TERRAIN") {
        f.terrain_z = raw();
        f.step = num() != 0;
        f.step_x = raw();
        f.step_h = raw();
        --ops;
        continue;
      }
      if (op == "SKILLSTAT") {
        f.skill_stat = raw();
        --ops;
        continue;
      }
      if (op == "CLIP") {
        auto token = raw();
        Fixture::Clip clip;
        clip.length = unsigned(num());
        auto count = num();
        for (std::uint64_t k = 0; k < count; ++k) {
          auto t = unsigned(num());
          clip.events[t] = raw();
        }
        f.clips[token] = clip;
        --ops;
        continue;
      }
      if (op == "ROOT") {
        auto a = C::Address(num());
        auto rx = raw(), ry = raw();
        f.root[a + 0x114] = {rx, ry};
        --ops;
        continue;
      }
      if (op == "RAY") {
        f.ray = raw();
        --ops;
        continue;
      }
      if (op == "YAW") {
        f.yaw = raw();
        --ops;
        continue;
      }
      if (op == "DIALOG") {
        f.dialog = num() != 0;
        --ops;
        continue;
      }
      if (op == "TRAVEL") {
        f.travel = raw();
        --ops;
        continue;
      }
      if (op == "CLASS") {
        s >> f.loot_class;
        if (f.loot_class == "-")
          f.loot_class.clear();
        --ops;
        continue;
      }
      // ---- operations -----------------------------------------------------------
      try {
        if (op == "S") {
          C::Spawn v;
          v.position = v3(s);
          v.facing = raw();
          v.hp = raw();
          v.team = s32(raw());
          v.creature_flags = num();
          v.armor = s32(raw());
          v.minimum = s32(raw());
          v.maximum = s32(raw());
          v.attack = s32(raw());
          v.home_range = raw();
          auto want = raw();
          check(w.spawn_skelight(v) == want, "skelight address");
        } else if (op == "P") {
          C::Spawn v;
          auto slot = unsigned(num());
          v.position = v3(s);
          v.facing = raw();
          v.hp = raw();
          auto level = int(num());
          auto strength = int(num());
          v.armor = s32(raw());
          v.team = s32(raw());
          v.idle_voice_limit = raw();
          v.speed_mul = raw();
          auto want = raw();
          auto got = w.spawn_player(slot, v, strength, level);
          f.players.insert(got);
          check(got == want, "player address");
        } else if (op == "W") {
          auto a = raw();
          auto z = unsigned(num());
          auto v = num();
          w.write(a, z, v);
        } else if (op == "LINK") {
          auto head = raw(), target = raw();
          w.relink(head, target);
        } else if (op == "DESTROY")
          w.destroy(raw());
        else if (op == "TICK")
          w.tick();
        else if (op == "STICK") {
          auto slot = unsigned(num());
          auto x = raw(), y = raw();
          w.stick(slot, x, y);
        } else if (op == "ATTACK") {
          auto slot = unsigned(num());
          w.press_attack(slot, num() != 0);
        } else if (op == "WEAPON") {
          auto slot = unsigned(num());
          C::Weapon v;
          v.minimum = s32(raw());
          v.maximum = s32(raw());
          v.attack = s32(raw());
          v.subtype = int(num());
          v.damage_type = raw();
          v.cls = int(num());
          auto count = num();
          for (std::uint64_t k = 0; k < count; ++k) {
            std::array<std::int32_t, 4> m;
            for (auto &x : m)
              x = s16(raw());
            v.mods.push_back(m);
          }
          w.give_weapon(slot, v);
        } else if (op == "SKILL") {
          auto id = int(num());
          C::SkillSetup v;
          v.kind = raw();
          v.recast = raw();
          v.flags = raw();
          for (auto &st : v.stats) {
            st.first = s16(raw());
            st.base = s16(raw());
            st.step = raw();
          }
          w.give_skill(id, v);
        } else if (op == "CASTQ") {
          auto caster = raw();
          auto id = int(num());
          auto level = int(num());
          std::optional<Vec3> point;
          std::optional<Raw> facing;
          if (num())
            point = v3(s);
          if (num())
            facing = raw();
          w.queue_cast(caster, id, level, point, facing);
        } else if (op == "MISSILE") {
          auto owner = raw();
          C::Missile m;
          m.position = v3(s);
          m.velocity = v3(s);
          m.facing = raw();
          m.minimum = raw();
          m.maximum = raw();
          m.attack = raw();
          m.damage_flags = raw();
          m.item = raw();
          m.kind = raw();
          m.launch = raw();
          m.impact = raw();
          m.rating = raw();
          m.item_flags = raw();
          m.models = {raw(), raw()};
          w.spawn_missile(owner, m);
        } else if (op == "THROWN") {
          auto owner = raw();
          C::Thrown t;
          t.position = v3(s);
          t.velocity = v3(s);
          t.facing = raw();
          t.minimum = raw();
          t.maximum = raw();
          t.attack = raw();
          t.kind = raw();
          t.item = raw();
          t.models = {raw(), raw()};
          w.spawn_thrown(owner, t);
        } else if (op == "CALL") {
          std::string name;
          s >> name;
          auto count = num();
          std::vector<Raw> a;
          for (std::uint64_t k = 0; k < count; ++k)
            a.push_back(raw());
          auto vec_at = [&](unsigned k) { return Vec3{a.at(k), a.at(k + 1), a.at(k + 2)}; };
          auto one = [&](std::uint64_t v) { r.result = {v}; };
          auto list = [&](const std::vector<C::Address> &v) {
            r.result.assign(v.begin(), v.end());
          };
          C::Arc arc;
          auto arc_from = [&](unsigned k) {
            arc.facing = a.at(k);
            arc.angle = a.at(k + 1);
            arc.minimum = a.at(k + 2);
            arc.maximum = a.at(k + 3);
            arc.attack = a.at(k + 4);
            arc.flags = a.at(k + 5);
            arc.near = a.at(k + 6);
            arc.far = a.at(k + 7);
          };
          if (name == "move_wc")
            one(w.move_with_collision(a.at(0), vec_at(1), a.at(4), a.at(5)));
          else if (name == "player_move")
            one(w.player_move(a.at(0), a.at(1), a.at(2), vec_at(3)));
          else if (name == "anim_step")
            one(w.player_anim_step(a.at(0), a.at(1)));
          else if (name == "clamp")
            one(w.clamp_to_screen(a.at(0), a.at(1), a.at(2)));
          else if (name == "pick") {
            w.pick_attack_anim(a.at(0), a.at(1));
            one(0);
          } else if (name == "melee") {
            w.player_melee_hit(a.at(0), a.at(1), a.at(2), a.at(3));
            one(0);
          } else if (name == "take") {
            w.take_damage(a.at(0), a.at(1), a.at(2), a.at(3));
            one(0);
          } else if (name == "process") {
            w.process_damage(a.at(0));
            one(0);
          } else if (name == "block_sparks") {
            w.block_sparks(a.at(0), a.at(1));
            one(0);
          } else if (name == "hit_sparks") {
            w.hit_sparks(a.at(0), a.at(1));
            one(0);
          } else if (name == "interact") {
            w.interact(a.at(0), a.at(1), a.at(2));
            one(0);
          } else if (name == "see_point")
            one(Raw(w.can_see_point(a.at(0), vec_at(1), a.at(4), a.at(5),
                                    a.at(6), a.at(7))));
          else if (name == "see_edge")
            one(Raw(w.can_see_edge(a.at(0), a.at(1), a.at(2), a.at(3), a.at(4),
                                   a.at(5), a.at(6), a.at(7))));
          else if (name == "see_obj")
            one(Raw(w.can_see(a.at(0), a.at(1), a.at(2), a.at(3), a.at(4),
                              a.at(5), a.at(6), a.at(7))));
          else if (name == "player_cone")
            one(w.find_player_in_cone(a.at(0), a.at(1), a.at(2), a.at(3),
                                      a.at(4)));
          else if (name == "enemy_cone")
            one(w.find_enemy_in_cone(a.at(0), a.at(1), a.at(2), a.at(3),
                                     a.at(4)));
          else if (name == "reach")
            one(Raw(w.hit_target_at_reach(a.at(0), a.at(1), a.at(2), a.at(3),
                                          a.at(4), a.at(5))));
          else if (name == "arc_edge") {
            arc_from(1);
            one(Raw(w.hit_target_in_arc_edge(a.at(0), arc)));
          } else if (name == "or_player") {
            arc_from(1);
            one(Raw(w.hit_target_or_player_in_arc(a.at(0), arc)));
          } else if (name == "arc_hit") {
            arc_from(1);
            one(Raw(w.hit_target_in_arc(a.at(0), arc)));
          } else if (name == "arc_all") {
            arc_from(1);
            one(Raw(w.hit_all_in_arc(a.at(0), arc)));
          } else if (name == "knock_target") {
            w.knockback_target(a.at(0), a.at(1));
            one(0);
          } else if (name == "cooldown") {
            w.set_cooldown(a.at(0), a.at(1));
            one(0);
          } else if (name == "cooldown_on") {
            w.set_cooldown_on(a.at(0), a.at(1), a.at(2));
            one(0);
          } else if (name == "notify") {
            w.notify_objects_near(a.at(0), vec_at(1), a.at(4), a.at(5));
            one(0);
          } else if (name == "arc_q")
            list(w.collect_in_arc(vec_at(0), a.at(3), a.at(4), a.at(5),
                                  a.at(6), a.at(7), a.at(8)));
          else if (name == "sphere_q")
            list(w.collect_in_sphere(vec_at(0), a.at(3), a.at(4)));
          else if (name == "radius_dmg")
            list(w.damage_in_radius(a.at(0), vec_at(1), a.at(4), a.at(5),
                                    a.at(6), a.at(7), a.at(8), a.at(9)));
          else if (name == "dead") {
            w.dead_update(a.at(0), a.at(1), a.at(2), a.at(3), vec_at(4));
            one(0);
          } else if (name == "drop_loot") {
            w.drop_loot(a.at(0), vec_at(1), a.at(4), a.at(5), a.at(6), a.at(7),
                        a.at(8), a.at(9), a.at(10), a.at(11));
            one(0);
          } else if (name == "goblin") {
            w.goblin_set_state(a.at(0), a.at(1));
            one(0);
          } else if (name == "damage")
            one(Raw(w.damage(a.at(0), a.at(1), a.at(2), a.at(3), a.at(4),
                             a.at(5))));
          else if (name == "knockback") {
            w.apply_knockback(a.at(0), a.at(1));
            one(0);
          } else if (name == "find_target") {
            w.player_find_target(a.at(0), a.at(1));
            one(0);
          } else
            throw std::runtime_error("unknown call " + name);
        } else
          throw std::runtime_error("unknown operation " + op);
        r.last_refused = false;
      } catch (const Mismatch &) {
        throw;
      } catch (const std::exception &e) {
        r.last_refused = true;
        r.refusal = e.what();
      }
      if (r.last_refused) {
        // The vector must say REFUSED next.
        std::streampos at = in.tellg();
        std::string next;
        if (!std::getline(in, next) || next != "REFUSED")
          throw std::runtime_error("native refused an operation the reference "
                                   "completed: " + r.refusal);
        ++n;
        (void)at;
        r.dead = true;
        r.last_refused = false;
        ++checks;
      }
    } catch (const std::exception &e) {
      throw std::runtime_error(r.label + " line " + std::to_string(n) + " " +
                               op + ": " + e.what());
    }
  }
  std::cout << cases << " cases, " << ops << " operations, " << checks
            << " exact comparisons passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      std::cerr << "usage: champions-combat-runtime-2-tests vectors.txt\n";
      return 2;
    }
    vectors(argv[1]);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
