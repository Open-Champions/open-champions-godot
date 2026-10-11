#include "champions/combat_damage.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <type_traits>
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::combat;
namespace {
struct CallbackError : std::runtime_error {
  CallbackError() : std::runtime_error("synthetic callback") {}
};
std::vector<std::string> split(const std::string &s, char c) {
  std::vector<std::string> out;
  std::size_t at = 0;
  for (;;) {
    auto end = s.find(c, at);
    out.push_back(s.substr(at, end == s.npos ? end : end - at));
    if (end == s.npos)
      return out;
    at = end + 1;
  }
}
std::int64_t number(const std::string &s) {
  std::size_t end = 0;
  auto n = std::stoll(s, &end);
  if (end != s.size())
    throw std::runtime_error("invalid integer");
  return n;
}
std::vector<std::int64_t> numbers(const std::string &s) {
  std::vector<std::int64_t> out;
  if (s.empty() || s == "-")
    return out;
  for (auto &item : split(s, ','))
    out.push_back(number(item));
  return out;
}
Text text(const std::string &s) { return Text(s.begin(), s.end()); }
std::string enc(const Text &s) {
  const char *digits = "0123456789abcdef";
  std::string out = "s";
  for (auto c : s) {
    if (c > 127)
      throw std::runtime_error("fixture nonASCII text");
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}
std::string enc(bool n) { return n ? "b1" : "b0"; }
template <class I,
          std::enable_if_t<std::is_integral_v<I> && !std::is_same_v<I, bool>,
                           int> = 0>
std::string enc(I n) {
  return std::to_string(n);
}
std::string pack(const std::vector<std::string> &fields) {
  std::string out = "[";
  for (auto &f : fields) {
    if (out.size() > 1)
      out += ',';
    out += f;
  }
  return out + "]";
}
template <class T> std::string enc(const std::vector<T> &v) {
  std::vector<std::string> result;
  for (auto x : v)
    result.push_back(enc(x));
  return pack(result);
}
std::string enc(const EventValue &value) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return "n";
        else
          return enc(v);
      },
      value.value);
}
struct Fixture {
  World w;
  std::array<Obj, 3> objects;
  std::vector<std::unique_ptr<Spell>> spells;
  std::vector<std::int64_t> stats{0, 0, 0};
  std::int64_t mode = 0, fail_at = 0, pool_ok = 1, enemy = 1, calls = 0;
  Obj *object(std::int64_t id) {
    return id < 0 ? nullptr : &objects.at(static_cast<std::size_t>(id));
  }
  std::int64_t identity(const Obj *p) const {
    if (!p)
      return -1;
    for (unsigned i = 0; i < 3; ++i)
      if (p == &objects[i])
        return i;
    throw std::runtime_error("unknown identity");
  }
  EventValue tag(const Obj *p) {
    return p ? EventValue(p->tag) : EventValue(nullptr);
  }
  template <class... Args> void callback(const char *name, Args &&...args) {
    ++calls;
    w.log(text(std::string("cb_") + name), std::forward<Args>(args)...);
    if (mode == 1) {
      objects[0].armor = -123;
      objects[0].res.at(0) = HALF;
      objects[0].hp = 0x41200000;
      objects[1].kind = 16;
    }
    if (mode == 2) {
      w.seed.draw();
      w.cosmetic.draw();
    }
    if (fail_at == calls)
      throw CallbackError();
  }
  explicit Fixture(const std::string &setup) {
    for (unsigned i = 0; i < 3; ++i) {
      objects[i].tag = Text(1, U"vao"[i]);
      objects[i].flags = i ? 96 : 32;
      objects[i].slot = i;
      PlayerRec rec;
      rec.obj = &objects[i];
      rec.hp = 0x42480000;
      rec.max_hp = 0x42c80000;
      w.players.push_back(rec);
    }
    objects[0].hp = objects[0].max_hp = objects[1].hp = objects[1].max_hp =
        0x42c80000;
    objects[1].pos = {ONE, 0, 0};
    w.alive = {0, 1, 2};
    for (auto &command : split(setup, ';')) {
      if (command.empty())
        continue;
      const auto pos = command.find('=');
      if (pos == command.npos)
        throw std::runtime_error("bad fixture command");
      const auto key = command.substr(0, pos), value = command.substr(pos + 1);
      if (key == "spell") {
        auto p = split(value, ':');
        if (p.size() != 9)
          throw std::runtime_error("bad fixture spell");
        auto s = std::make_unique<Spell>();
        s->kind = text(p[1]);
        s->owner = object(number(p[2]));
        s->skill = number(p[3]);
        s->level = number(p[4]);
        s->f18 = number(p[5]);
        s->f1c = number(p[6]);
        s->f20 = number(p[7]);
        s->f2c = number(p[8]);
        object(number(p[0]))->spells.push_back(s.get());
        spells.push_back(std::move(s));
      } else if (key == "skill") {
        auto p = numbers(value);
        w.players.at(p.at(0)).skills.emplace_back(p.at(1), p.at(2));
      } else if (key == "seed")
        w.seed.state = number(value);
      else if (key == "cosmetic")
        w.cosmetic.state = number(value);
      else if (key == "stats")
        stats = numbers(value);
      else if (key == "mode")
        mode = number(value);
      else if (key == "fail_at")
        fail_at = number(value);
      else if (key == "pool_ok")
        pool_ok = number(value);
      else if (key == "enemy")
        enemy = number(value);
      else {
        auto path = split(key, '.');
        if (path.size() != 2)
          throw std::runtime_error("bad field path");
        const auto &name = path[1];
        if (path[0] == "w") {
          if (name == "alive")
            w.alive = numbers(value);
          else if (name == "focus") {
            auto n = numbers(value);
            w.focus.assign(n.begin(), n.end());
          } else if (name == "music")
            w.music = numbers(value);
#define WORLD_FIELD(field) else if (name == #field) w.field = number(value)
          WORLD_FIELD(frame);
          WORLD_FIELD(pvp_a);
          WORLD_FIELD(cutscene);
          WORLD_FIELD(numbers_option);
          WORLD_FIELD(network_mode);
          WORLD_FIELD(music_index);
          WORLD_FIELD(monster_level);
#undef WORLD_FIELD
          else throw std::runtime_error("unknown world field");
        } else if (path[0][0] == 'p') {
          auto &rec = w.players.at(number(path[0].substr(1)));
          if (name == "hp")
            rec.hp = number(value);
          else if (name == "max_hp")
            rec.max_hp = number(value);
          else
            throw std::runtime_error("unknown player field");
        } else {
          const auto index = std::string("vao").find(path[0]);
          if (index > 2)
            throw std::runtime_error("unknown object");
          auto &obj = objects.at(index);
          if (name == "cls")
            obj.cls = text(value);
          else if (name == "owner")
            obj.owner = object(number(value));
          else if (name == "status_source")
            obj.status_source = object(number(value));
          else if (name == "owner_link")
            obj.owner_link = number(value) != 0;
          else if (name == "flash")
            obj.flash = numbers(value);
#define LIST_FIELD(field)                                                      \
  else if (name == #field) {                                                   \
    auto n = numbers(value);                                                   \
    obj.field.assign(n.begin(), n.end());                                      \
  }
          LIST_FIELD(pos)
          LIST_FIELD(res)
          LIST_FIELD(status)
#undef LIST_FIELD
#define OBJ_FIELD(field) else if (name == #field) obj.field = number(value)
          OBJ_FIELD(flags);
          OBJ_FIELD(cflags);
          OBJ_FIELD(facing);
          OBJ_FIELD(kind);
          OBJ_FIELD(armor);
          OBJ_FIELD(hp);
          OBJ_FIELD(max_hp);
          OBJ_FIELD(dot);
          OBJ_FIELD(counter11);
          OBJ_FIELD(counter12);
          OBJ_FIELD(stamp_f0);
          OBJ_FIELD(slot);
          OBJ_FIELD(state);
          OBJ_FIELD(model_points);
          OBJ_FIELD(boss_rank);
          OBJ_FIELD(innoruuk_immune);
          OBJ_FIELD(poison_a);
          OBJ_FIELD(poison_b);
#undef OBJ_FIELD
          else throw std::runtime_error("unknown object field");
        }
      }
    }
    w.stat_fn = [this](auto skill, auto stat, auto level) {
      callback("stat", skill, stat, level);
      return stats.at(stat % 3);
    };
    w.stat_for_fn = [this](auto skill, auto stat, auto level, Obj *caster,
                           auto cap) {
      callback("stat_for", skill, stat, level, tag(caster), cap);
      return stats.at(stat % 3);
    };
    w.adjust_fn = [this](auto skill, auto level, auto n) {
      callback("adjust", skill, level, n);
      return level;
    };
    w.pool_fn = [this](auto size) {
      callback("pool", size);
      return pool_ok != 0;
    };
    w.enemy_fn = [this](Obj *a, Obj *b) {
      callback("enemy", a->tag, b->tag);
      return enemy;
    };
    w.take_damage_fn = [this](Obj &x, Raw amount, Obj *source,
                              std::uint32_t flags) {
      callback("take", x.tag, amount, tag(source), flags);
      if (mode == 3) {
        x.hp = fsub(x.hp, amount);
        x.flags |= 8;
        w.players[1].hp = fsub(w.players[1].hp, ONE);
      }
    };
  }
  std::string state() const {
    if (w.has_shared_random())
      throw std::runtime_error(
          "legacy combat trace cannot omit shared random authority");
    std::vector<std::string> obs, players, events;
    for (const auto &x : objects) {
      std::vector<std::string> sp;
      for (auto *s : x.spells)
        sp.push_back(pack({enc(s->kind), enc(s->skill), enc(s->level),
                           enc(identity(s->owner)), enc(s->f18), enc(s->f1c),
                           enc(s->f20), enc(s->f2c)}));
      obs.push_back(pack({enc(x.tag),
                          enc(x.flags),
                          enc(x.pos),
                          enc(x.facing),
                          enc(x.owner_link),
                          enc(identity(x.owner)),
                          enc(x.cls),
                          enc(x.innoruuk_immune),
                          enc(x.res),
                          enc(x.flash),
                          enc(x.cflags),
                          enc(x.kind),
                          enc(x.dot),
                          enc(x.armor),
                          enc(x.stamp_f0),
                          enc(x.hp),
                          enc(x.max_hp),
                          enc(x.status),
                          enc(x.poison_a),
                          enc(x.poison_b),
                          enc(identity(x.status_source)),
                          pack(sp),
                          enc(x.counter11),
                          enc(x.counter12),
                          enc(x.slot),
                          enc(x.state),
                          enc(x.model_points),
                          enc(x.boss_rank)}));
    }
    for (const auto &p : w.players) {
      std::vector<std::string> skills;
      for (auto skill : p.skills)
        skills.push_back(pack({enc(skill.first), enc(skill.second)}));
      players.push_back(pack({enc(p.hp), enc(p.max_hp), pack(skills)}));
    }
    for (const auto &event : w.events) {
      std::vector<std::string> fields;
      for (auto &field : event)
        fields.push_back(enc(field));
      events.push_back(pack(fields));
    }
    return pack({enc(w.seed.state), enc(w.seed.draws), enc(w.cosmetic.state),
                 enc(w.cosmetic.draws), enc(w.music), pack(obs), pack(players),
                 enc(calls), pack(events)});
  }
  std::string run(const std::string &op, const std::vector<std::int64_t> &p) {
    auto &v = objects[0];
    auto &a = objects[1];
    try {
      if (op == "base_roll")
        return enc(base_roll(p.at(0), p.at(1), p.at(2)));
      if (op == "proc_amount")
        return enc(proc_amount(w, p.at(0), p.at(1)));
      if (op == "status")
        set_status(w, v, p.at(0), p.at(1), object(p.at(2)));
      else if (op == "fx")
        fx(w, p.at(0), v);
      else if (op == "bloodlust")
        return enc(bloodlust_armor_factor(w, required(v.spells.at(0))));
      else if (op == "spirit")
        return enc(spirit_shield_absorbs(w, required(v.spells.at(0))));
      else if (op == "frost")
        return enc(frost_armor_on_hit(w, required(v.spells.at(0)), p.at(0),
                                      object(p.at(1))));
      else if (op == "thorns")
        thorns(w, p.at(0), v, a, p.at(1));
      else if (op == "crit")
        return enc(crit_bonus(w, p.at(0), a, p.at(1)));
      else if (op == "stun_chance")
        return enc(stun_chance(w, v, p.at(0)));
      else if (op == "stun_effect")
        stun_effect(w, v);
      else if (op == "retribution")
        return enc(retribution(w, required(v.spells.at(0)), p.at(0)));
      else if (op == "colour")
        return enc(colour_class(v, p.at(0)));
      else if (op == "facing")
        return enc(facing_front(v, a));
      else if (op == "damage") {
        Descriptor desc;
        for (std::size_t i = 6; i < p.size(); i += 4)
          desc.push_back({p.at(i), p.at(i + 1), p.at(i + 2), p.at(i + 3)});
        return enc(damage_apply(w, v, object(p.at(5)), p.at(0), p.at(1),
                                p.at(2), p.at(3),
                                p.size() == 6 ? nullptr : &desc, p.at(4)));
      } else
        throw std::runtime_error("unknown vector operation");
      return "n";
    } catch (const CallbackError &) {
      return "!CallbackError";
    } catch (const Unsupported &) {
      throw;
    } catch (const Error &) {
      return "!Error";
    } catch (const std::out_of_range &) {
      return "!Error";
    }
  }
};
void anchor(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void anchors() {
  set_fpu();
  anchor(base_roll(ONE, ONE, 0xffffffff) == ONE, "unsigned draw modulo anchor");
  Fixture fixture("spell=0:FrostArmor:0:7:1:0:0:0:0;stats=0,0,100");
  bool exhausted = false;
  try {
    damage_apply(fixture.w, fixture.objects[0], &fixture.objects[0], ONE, ONE,
                 100000, 0);
  } catch (const Unsupported &) {
    exhausted = true;
  }
  anchor(exhausted, "infinite reflection must explicitly fail");
  anchor(fixture.w.seed.draws.size() > 100,
         "exhaustion retains actual draw history");
  Fixture fresh("");
  anchor(damage_apply(fresh.w, fresh.objects[0], nullptr, ONE, ONE, 100000,
                      0) == 1,
         "depth restored after exception");
  Obj v;
  Obj source;
  v.status_source = &source;
  World w;
  set_status(w, v, 0, ZERO, nullptr);
  anchor(v.status_source == &source, "zero status retains source identity");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error(
          "usage: combat-damage-tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
      throw std::runtime_error("missing vector input");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw std::runtime_error("cannot create trace");
    }
    std::string line;
    std::size_t count = 0;
    std::uint64_t hash = 14695981039346656037ULL;
    std::map<std::pair<std::string, int>, std::size_t> counts;
    if (!std::getline(input, line) || line != "# combat_damage_vectors_v1")
      throw std::runtime_error("missing vector schema");
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      auto fields = split(line, '\t');
      if (fields.size() != 5 || fields[0] != "damage")
        throw std::runtime_error("malformed vector");
      auto args = split(fields[2], '|');
      if (args.size() != 3)
        throw std::runtime_error("malformed arguments");
      int config = static_cast<int>(number(args[0]));
      if (config < 0 || config >= 16)
        throw std::runtime_error("bad FPU config");
      set_fpu(static_cast<Fpu::Model>(config / 4), (config & 2) != 0,
              (config & 1) != 0);
      Fixture fixture(args[2]);
      auto result = fixture.run(fields[1], numbers(args[1]));
      auto state = fixture.state();
      if (result != fields[3] || state != fields[4]) {
        std::cerr << "Mismatch row " << count + 1 << " op " << fields[1]
                  << " args " << fields[2] << "\nexpected result " << fields[3]
                  << " actual " << result << "\nexpected state " << fields[4]
                  << "\nactual state " << state << '\n';
        return 1;
      }
      const auto actual = fields[0] + "\t" + fields[1] + "\t" + fields[2] +
                          "\t" + result + "\t" + state + "\n";
      for (unsigned char byte : actual) {
        hash ^= byte;
        hash *= 1099511628211ULL;
      }
      if (trace)
        trace << actual;
      ++count;
      ++counts[{fields[1], config}];
    }
    if (input.bad())
      throw std::runtime_error("vector read failure");
    // Exact corpus identity catches changed, repeated, missing and truncated
    // data even when a modified expected value would otherwise compare equal.
    constexpr std::uint64_t expected_hash = 0x8e2ec8264ef1a8faULL;
    constexpr std::size_t expected_count = 44704;
    if (count != expected_count || hash != expected_hash)
      throw std::runtime_error("incomplete or changed damage corpus");
    if (trace) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace write failure");
    }
    std::cout << count << " exact damage vectors passed; FNV1a64 " << std::hex
              << hash << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
