// Authored, asset-free combat-to-creature replay. The native runtime computes
// every value; immutable Python-model fixtures supply independent expectations.
#include "champions/combat_creature.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    const auto end = s.find(separator, start);
    result.push_back(s.substr(start, end == s.npos ? end : end - start));
    if (end == s.npos)
      return result;
    start = end + 1;
  }
}
std::int64_t integer(const std::string &s) {
  if (s.empty() || s == "-0" || s.front() == '+' ||
      (s.size() > 1 && s.front() == '0') ||
      (s.size() > 2 && s[0] == '-' && s[1] == '0'))
    throw std::runtime_error("noncanonical integer");
  for (std::size_t i = s.front() == '-' ? 1 : 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9')
      throw std::runtime_error("invalid integer");
  std::size_t used = 0;
  const auto value = std::stoll(s, &used);
  if (used != s.size())
    throw std::runtime_error("invalid integer");
  return value;
}
std::string sequence(const std::vector<std::string> &values) {
  std::string out = "[";
  for (const auto &value : values) {
    if (out.size() > 1)
      out += ',';
    out += value;
  }
  return out + ']';
}
template <class C> std::string bytes(const C &values) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "b";
  for (const auto value : values) {
    const auto ch = static_cast<unsigned char>(value);
    out += hex[ch >> 4];
    out += hex[ch & 15];
  }
  return out;
}
std::string text(const std::string &value) {
  auto out = bytes(value);
  out.front() = 's';
  return out;
}
template <class C> std::string integers(const C &values) {
  std::vector<std::string> out;
  for (const auto value : values)
    out.push_back(std::to_string(value));
  return sequence(out);
}

namespace R = champions::simulation::rta;
namespace C = R::combat;
namespace H = R::creature_hp;
namespace B = R::combat_creature;
std::string n(std::uint64_t value) { return std::to_string(value); }
std::string number(std::int64_t value) { return std::to_string(value); }
std::string text(const C::Text &value) {
  std::string out;
  for (std::uint32_t c : value) {
    if (c <= 0x7f)
      out += char(c);
    else if (c <= 0x7ff) {
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
    } else if (c <= 0xffff) {
      out += char(0xe0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else if (c <= 0x10ffff) {
      out += char(0xf0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 63));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else
      throw std::runtime_error("invalid text code point");
  }
  return text(out);
}
std::string event_value(const C::EventValue &event) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return "_";
        else if constexpr (std::is_same_v<T, C::Text>)
          return text(v);
        else if constexpr (std::is_same_v<T, std::vector<R::Raw>>)
          return integers(v);
        else
          return std::to_string(v);
      },
      event.value);
}
std::string write(const H::Write &value) {
  return sequence({n(value.address), n(value.size), n(value.value)});
}
std::string call(const H::Call &value) {
  return sequence({text(value.name), integers(value.args)});
}
constexpr std::array<std::uint32_t, 3> addresses{0x10000, 0x20000, 0x30000};
constexpr std::array<std::uint32_t, 2> seeds{1, 0xdeadbeef};
const std::array<std::string, 36> names{"normal",
                                        "zero",
                                        "negative",
                                        "repeat_lethal",
                                        "inert",
                                        "ignore",
                                        "combat_status",
                                        "projectile",
                                        "effect",
                                        "null_node",
                                        "null_target",
                                        "stale_target",
                                        "shared_handles",
                                        "self",
                                        "reflection",
                                        "frost",
                                        "thorns",
                                        "dot_delivery",
                                        "dot_pending",
                                        "miss",
                                        "raw_hp_mutation",
                                        "raw_route_mutation",
                                        "clear_throw",
                                        "link_throw",
                                        "nested_callback",
                                        "nested_throw",
                                        "ignore_status",
                                        "custom",
                                        "no_projection",
                                        "status_publish",
                                        "callback_rng",
                                        "frame_mutation",
                                        "effect_null_node",
                                        "effect_null_target",
                                        "reflection_throw",
                                        "nested_damage"};
struct CallbackFailure : std::runtime_error {
  CallbackFailure() : std::runtime_error("authored callback failure") {}
};
struct Replay {
  B::Adapter adapter;
  std::string name;
  unsigned variant;
  std::array<C::Obj *, 3> objects{};
  std::vector<C::Spell *> spells;
  std::vector<std::string> effects, pokes, results;
  std::size_t event_cursor = 0;
  unsigned callback_count = 0;
  bool nested = false;
  Replay(std::string name_, unsigned v)
      : adapter(seeds[v], seeds[1 - v]), name(std::move(name_)), variant(v) {
    const std::array<C::Text, 3> tags{U"victim", U"attacker", U"owner"};
    for (unsigned i = 0; i < 3; ++i) {
      C::Obj obj;
      obj.tag = tags[i];
      obj.flags = 32;
      obj.hp = obj.max_hp = 0x42c80000;
      obj.pos = {R::from_int(i + 1), R::from_int(v), 0};
      obj.own_seed = 0xabcdef00 + i;
      objects[i] = &adapter.create(
          addresses[i], obj,
          name == "no_projection" && i == 0 ? B::Projection::None
                                            : B::Projection::CreatureFields,
          name == "no_projection" && i == 0 ? B::Dispatch::Custom
                                            : B::Dispatch::BaseCreature,
          0x3b2);
    }
    adapter.reserve_raw(0x50000, 16, "owner");
    adapter.reserve_raw(0x50100, 16, "effect");
    adapter.reserve_raw(0x51000, 48, "last_handles");
    adapter.set_handle_callback("handle_clear", "authored_clear",
                                [this](B::Adapter &, const auto &args) {
                                  handle("handle_clear", args);
                                });
    adapter.set_handle_callback("handle_link", "authored_link",
                                [this](B::Adapter &, const auto &args) {
                                  handle("handle_link", args);
                                });
    auto &w = adapter.world();
    w.stat_fn = [this](auto skill, auto stat, auto level) {
      return stat_value("stat", skill, stat, level, {});
    };
    w.stat_for_fn = [this](auto skill, auto stat, auto level, C::Obj *caster,
                           auto cap) {
      return stat_value("stat_for", skill, stat, level,
                        {ident(caster), number(cap)});
    };
    w.adjust_fn = [](auto, auto level, auto) { return level; };
    w.pool_fn = [](auto) { return false; };
    w.enemy_fn = [](auto *, auto *) { return 1; };
    w.frame = 32 + v;
    w.numbers_option = 0;
    w.level_name = U"authored";
    adapter.project();
    if (name == "no_projection") {
      const auto &o = *objects[0];
      p(addresses[0] + 0xc, 4, o.flags);
      for (unsigned i = 0; i < 3; ++i)
        p(addresses[0] + 0x10 + 4 * i, 4, o.pos[i]);
      p(addresses[0] + 0xa8, 8, o.cflags);
      p(addresses[0] + 0x228, 4, o.hp);
      for (unsigned i = 0; i < 5; ++i)
        p(addresses[0] + 0x340 + 4 * i, 4, o.status[i]);
    }
    for (auto a : addresses) {
      p(a + 0xc8, 4, R::HALF);
      p(a + 0x231, 1, 0);
      p(a + 0x2dc, 4, v);
      p(a + 0x2e0, 2, 0x1234);
      p(a + 0x2e4, 4, 0);
      p(a + 0x304, 4, 7);
      p(a + 0x3b0, 2, 9);
      p(a + 0x4c, 4, 0);
      p(a + 0x360, 4, 0);
    }
    for (const auto &region : adapter.raw_regions())
      for (unsigned offset = 0; offset < region.size; offset += 4)
        p(region.address + offset, 4, 0);
  }
  std::string ident(const C::Obj *obj) const {
    if (!obj)
      return "_";
    for (unsigned i = 0; i < objects.size(); ++i)
      if (objects[i] == obj)
        return n(i);
    throw std::runtime_error("foreign object in trace");
  }
  std::string spell_ident(const C::Spell *s) const {
    if (!s)
      return "_";
    for (unsigned i = 0; i < spells.size(); ++i)
      if (spells[i] == s)
        return n(i);
    throw std::runtime_error("foreign spell in trace");
  }
  std::uint32_t address(const C::Obj *o) const {
    return o ? adapter.binding(*o).address : 0;
  }
  void p(std::uint64_t a, std::size_t size, std::uint64_t value) {
    adapter.model().mem.poke(a, size, value);
  }
  void flush() {
    const auto &events = adapter.model().events;
    while (event_cursor < events.size()) {
      const auto &e = events[event_cursor++];
      if (e.kind == H::EventKind::Write)
        effects.push_back(sequence({text("write"), n(e.write.address),
                                    n(e.write.size), n(e.write.value)}));
      else if (e.kind == H::EventKind::Call)
        effects.push_back(
            sequence({text("call"), text(e.call.name), integers(e.call.args)}));
      else
        throw std::runtime_error("unknown HP event");
    }
  }
  void poke(std::uint64_t a, std::size_t size, std::uint64_t value) {
    flush();
    if (size < 8)
      value &= (UINT64_C(1) << (8 * size)) - 1;
    pokes.push_back(sequence({n(callback_count), n(a), n(size), n(value)}));
    effects.push_back(sequence({text("poke"), n(a), n(size), n(value)}));
    p(a, size, value);
  }
  std::int64_t stat_value(const std::string &label, std::int64_t skill,
                          std::int64_t stat, std::int64_t level,
                          std::vector<std::string> extra) {
    flush();
    std::vector<std::string> values{text(label), number(skill), number(stat),
                                    number(level)};
    values.insert(values.end(), extra.begin(), extra.end());
    effects.push_back(sequence(values));
    if (name == "frost")
      return std::array<int, 3>{0, 25, 100}[std::size_t(stat % 3)];
    if (name == "thorns")
      return std::array<int, 3>{3, 100, 0}[std::size_t(stat % 3)];
    return 0;
  }
  template <class F>
  void invoke(const std::string &label, F function, bool catch_error = true) {
    try {
      auto value = function();
      results.push_back(sequence({text(label), text("ok"), value}));
    } catch (const CallbackFailure &) {
      results.push_back(sequence({text(label), text("CallbackFailure"), "_"}));
      if (!catch_error)
        throw;
    }
  }
  void handle(const std::string &label,
              const std::vector<std::uint32_t> &args) {
    flush();
    ++callback_count;
    effects.push_back(sequence(
        {text("handler"), text(label), n(callback_count), integers(args)}));
    const auto head = args.at(0);
    if (label == "handle_clear") {
      poke(head, 4, 0);
      if (name == "raw_hp_mutation") {
        poke(addresses[0] + 0x228, 4, 0x41200000);
        poke(addresses[0] + 0xc8, 4, R::TWO);
      }
      if (name == "raw_route_mutation") {
        poke(addresses[1] + 0xc, 4, 0xa0);
        poke(addresses[1] + 0x4c, 4, 0x50000);
        poke(0x5000c, 4, addresses[2]);
      }
      if (name == "clear_throw")
        throw CallbackFailure();
      if ((name == "nested_callback" || name == "nested_throw" ||
           name == "nested_damage") &&
          !nested) {
        nested = true;
        poke(addresses[1] + 0x228, 4, 0x41a00000);
        invoke(
            "nested",
            [&] {
              if (name == "nested_damage") {
                C::Descriptor desc = C::EMPTY_DESC;
                desc[0] = {0x15, 2, 3, 0};
                return number(adapter.damage(*objects[1], objects[2], R::TWO,
                                             C::FIVE, 100000, 0x40, &desc, 1));
              }
              adapter.take_damage(*objects[1], R::TWO, objects[2], 0x40);
              return std::string("_");
            },
            false);
      }
      if (name == "frame_mutation")
        poke(0x44fdcc, 4, 99 + variant);
    } else {
      const auto index = (head - 0x2e4 - addresses[0]) / 0x10000;
      const auto node = 0x51000 + 16 * index;
      poke(node + 0xc, 4, args.at(1));
      poke(head, 4, node);
      if (name == "link_throw" || name == "reflection_throw" ||
          (name == "nested_throw" && nested))
        throw CallbackFailure();
    }
    if (name == "callback_rng") {
      adapter.world().seed.draw();
      adapter.world().cosmetic.draw();
    }
  }
  std::string object(const C::Obj &o) const {
    std::vector<std::string> spellrefs;
    for (auto *s : o.spells)
      spellrefs.push_back(spell_ident(s));
    return sequence({text(o.tag),
                     n(o.flags),
                     integers(o.pos),
                     number(o.facing),
                     n(o.owner_link),
                     ident(o.owner),
                     text(o.cls),
                     number(o.innoruuk_immune),
                     integers(o.res),
                     integers(o.flash),
                     n(o.cflags),
                     number(o.kind),
                     n(o.dot),
                     n(o.field_dc),
                     number(o.armor),
                     number(o.stamp_f0),
                     n(o.hp),
                     n(o.max_hp),
                     integers(o.status),
                     number(o.poison_a),
                     number(o.poison_b),
                     ident(o.status_source),
                     sequence(spellrefs),
                     number(o.counter2),
                     number(o.counter4),
                     number(o.counter11),
                     number(o.counter12),
                     number(o.slot),
                     number(o.state),
                     number(o.attack_start),
                     number(o.attack_chain),
                     integers(o.bonus),
                     number(o.model_points),
                     number(o.boss_rank),
                     n(o.exp_value),
                     n(o.own_seed),
                     ident(o.last_attacker)});
  }
  std::string player(const C::PlayerRec &p) const {
    auto emptyrefs = [](const std::vector<C::Item *> &items) {
      std::vector<std::string> out;
      for (auto *i : items) {
        if (i)
          throw std::runtime_error("unexpected item registry");
        out.push_back("_");
      }
      return sequence(out);
    };
    if (p.second_weapon)
      throw std::runtime_error("unexpected second weapon");
    std::vector<std::string> skills;
    for (const auto &s : p.skills)
      skills.push_back(sequence({number(s.first), number(s.second)}));
    return sequence({number(p.cls),
                     number(p.level58),
                     number(p.level59),
                     number(p.exp),
                     integers(p.base),
                     integers(p.eff),
                     n(p.hp),
                     n(p.max_hp),
                     n(p.mana),
                     n(p.max_mana),
                     number(p.attack),
                     number(p.dmg_min),
                     number(p.dmg_max),
                     n(p.stagger),
                     number(p.skill_points),
                     number(p.stat_points),
                     integers(p.passives),
                     n(p.load_limit),
                     emptyrefs(p.bag2),
                     ident(p.obj),
                     emptyrefs(p.equip),
                     emptyrefs(p.weapon_sets),
                     "_",
                     number(p.active_set),
                     sequence(skills)});
  }
  std::string snapshot() {
    flush();
    const auto &w = adapter.world();
    const auto &m = adapter.model();
    std::vector<std::string> objs, spellvalues, players, wevents, bindings,
        regions, labels, memory, writes, stores, calls;
    for (auto *o : objects)
      objs.push_back(object(*o));
    for (auto *s : spells)
      spellvalues.push_back(sequence(
          {text(s->kind), number(s->skill), number(s->level), ident(s->owner),
           number(s->f18), n(s->f1c), n(s->f20), number(s->f2c)}));
    for (const auto &p : w.players)
      players.push_back(player(p));
    for (const auto &e : w.events) {
      std::vector<std::string> values;
      for (const auto &v : e)
        values.push_back(event_value(v));
      wevents.push_back(sequence(values));
    }
    const auto world = sequence(
        {sequence({n(w.seed.state), integers(w.seed.draws)}),
         sequence({n(w.cosmetic.state), integers(w.cosmetic.draws)}),
         sequence({number(w.frame), number(w.pvp_a), number(w.cutscene),
                   number(w.numbers_option), number(w.network_mode),
                   n(w.monster_level), integers(w.focus), integers(w.music),
                   number(w.music_index), number(w.num_players),
                   integers(w.alive), number(w.medal_mission),
                   number(w.difficulty), text(w.level_name),
                   number(w.load_cheat)}),
         sequence(players), sequence(wevents),
         sequence({n(bool(w.stat_fn)), n(bool(w.stat_for_fn)),
                   n(bool(w.adjust_fn)), n(bool(w.pool_fn)),
                   n(bool(w.enemy_fn)), n(bool(w.take_damage_fn))})});
    for (const auto &b : adapter.bindings())
      bindings.push_back(sequence({n(b.address), n(b.generation), n(b.extent),
                                   n(unsigned(b.projection)),
                                   n(unsigned(b.dispatch)), text(b.label)}));
    for (const auto &r : adapter.raw_regions())
      regions.push_back(sequence({n(r.address), n(r.size), text(r.label)}));
    auto callbacks = adapter.callback_bindings();
    std::sort(callbacks.begin(), callbacks.end(),
              [](const auto &a, const auto &b) { return a.name < b.name; });
    for (const auto &c : callbacks)
      labels.push_back(sequence({text(c.name), text(c.label)}));
    for (const auto &e : m.mem.bytes())
      memory.push_back(sequence({n(e.first), n(e.second)}));
    for (const auto &e : m.writes)
      writes.push_back(write(e));
    for (const auto &e : m.mem.log())
      stores.push_back(write(e));
    for (const auto &e : m.calls)
      calls.push_back(call(e));
    return sequence(
        {sequence(results), world, sequence(objs), sequence(spellvalues), "[]",
         "[]", sequence(bindings), sequence(regions), sequence(labels),
         sequence(memory), sequence(writes), sequence(stores), sequence(calls),
         sequence(effects), sequence(pokes), n(m.draws),
         n(adapter.operation_depth()), n(adapter.raw_callback_depth())});
  }
  void recipe();
};

void Replay::recipe() {
  auto &v = *objects[0];
  auto *a = objects[1];
  auto *owner = objects[2];
  R::Raw amount = variant ? C::FIVE : 0x40800000;
  std::uint32_t flags = 0x40;
  std::function<std::string()> hit = [&] {
    adapter.take_damage(v, amount, a, flags);
    return std::string("_");
  };
  std::function<std::string()> fight = [&] {
    return number(
        adapter.damage(v, a, R::TWO, C::FIVE, 100000, flags, nullptr, 1));
  };
  auto action = hit;
  C::Descriptor descriptor = C::EMPTY_DESC;
  if (name == "zero")
    amount = variant ? 0x80000000 : 0;
  else if (name == "negative")
    amount = variant ? 0xc1200000 : 0xbf800000;
  else if (name == "repeat_lethal") {
    v.hp = R::ONE;
    action = [&] {
      adapter.take_damage(v, amount, a, flags);
      adapter.take_damage(v, amount, owner, flags);
      return std::string("[_,_]");
    };
  } else if (name == "inert")
    p(addresses[0] + 0x231, 1, variant ? 255 : 1);
  else if (name == "ignore") {
    v.cflags |= UINT64_C(0x1000000000000);
    for (unsigned i = 0; i < 5; ++i)
      v.status[i] = R::ONE + i;
  } else if (name == "combat_status" || name == "ignore_status") {
    if (name == "ignore_status")
      v.cflags |= UINT64_C(0x1000000000000);
    descriptor[0] = {0x15, 2, 3, 0};
    action = [&] {
      return number(
          adapter.damage(v, a, R::TWO, C::FIVE, 100000, flags, &descriptor, 1));
    };
  } else if (name == "projectile" || name == "null_node" ||
             name == "null_target" || name == "stale_target" ||
             name == "raw_route_mutation") {
    a->flags |= 0x80;
    a->owner_link = true;
    a->owner = owner;
    a->cls = U"Projectile";
    owner->cls = U"Owner";
    p(addresses[1] + 0x4c, 4, name == "null_node" ? 0 : 0x50000);
    p(0x5000c, 4,
      name == "null_target"    ? 0
      : name == "stale_target" ? 0xdead1000
                               : addresses[2]);
    flags |= 0x10000000;
    p(addresses[0] + 0x360, 4, 0x50100);
    p(0x5010c, 4, addresses[0]);
    if (name == "raw_route_mutation") {
      a->flags = 32;
      p(addresses[1] + 0x4c, 4, 0);
    }
  } else if (name == "effect" || name == "shared_handles" ||
             name == "effect_null_node" || name == "effect_null_target") {
    flags |= 0x10000000;
    v.status_source = owner;
    p(addresses[0] + 0x360, 4, name == "effect_null_node" ? 0 : 0x50100);
    p(0x5010c, 4, name == "effect_null_target" ? 0 : addresses[2]);
    if (name == "shared_handles") {
      p(addresses[1] + 0x360, 4, 0x50100);
      a->status_source = owner;
      action = [&] {
        adapter.take_damage(v, amount, a, flags);
        adapter.take_damage(*a, amount, &v, flags);
        return std::string("[_,_]");
      };
    }
  } else if (name == "self")
    action = [&] {
      adapter.take_damage(v, amount, &v, flags);
      return std::string("_");
    };
  else if (name == "reflection" || name == "reflection_throw") {
    v.counter12 = 1;
    a->kind = 0x10;
    action = fight;
  } else if (name == "frost") {
    C::Spell spell;
    spell.kind = U"FrostArmor";
    spell.skill = 7;
    spell.level = 1;
    spell.owner = &v;
    auto *s = adapter.world().add_owned_spell(spell);
    spells.push_back(s);
    v.spells.push_back(s);
    flags = 1;
    action = fight;
  } else if (name == "thorns") {
    v.cflags |= C::CF_THORNS;
    flags = 1;
    action = fight;
  } else if (name == "dot_delivery" || name == "dot_pending") {
    adapter.world().frame = (name == "dot_delivery" ? 32 : 33) + 32 * variant;
    flags = 0x800000;
    v.dot = 0x3fc00000;
    action = fight;
  } else if (name == "miss") {
    v.armor = 1;
    action = [&] {
      return number(
          adapter.damage(v, a, R::TWO, C::FIVE, 0, flags, nullptr, 1));
    };
  } else if (name == "custom" || name == "no_projection") {
    adapter.bind_custom(
        v, name == "custom" ? "authored_custom" : "authored_raw_custom",
        [this](B::Adapter &x, C::Obj &obj, R::Raw am, C::Obj *at,
               std::uint32_t fl) {
          flush();
          ++callback_count;
          effects.push_back(
              sequence({text("custom"), ident(&obj), n(am), ident(at), n(fl)}));
          if (name == "no_projection")
            x.model().take_damage(address(&obj), address(at), fl, am);
          else
            obj.hp = C::fsub(obj.hp, am);
        });
    if (name == "custom") {
      v.flags = 0;
      action = fight;
    } else {
      p(addresses[0] + 0x228, 4, 0x41200000);
      v.hp = 0x42f00000;
    }
  } else if (name == "status_publish") {
    adapter.set_before_base("authored_status", [this](B::Adapter &, C::Obj &obj,
                                                      R::Raw am, C::Obj *at,
                                                      std::uint32_t fl) {
      flush();
      ++callback_count;
      effects.push_back(
          sequence({text("before"), ident(&obj), n(am), ident(at), n(fl)}));
      poke(0x5010c, 4, address(obj.status_source));
      poke(address(&obj) + 0x360, 4, 0x50100);
    });
    C::set_status(adapter.world(), v, 0, C::FIVE, owner);
    flags = 0x10000000;
  } else if (name != "normal" && name != "raw_hp_mutation" &&
             name != "clear_throw" && name != "link_throw" &&
             name != "nested_callback" && name != "nested_throw" &&
             name != "nested_damage" && name != "callback_rng" &&
             name != "frame_mutation")
    throw std::runtime_error("unknown recipe");
  invoke("entry", action);
}

std::pair<std::string, std::string> scenario(const std::string &name,
                                             std::int64_t config,
                                             const std::string &arguments) {
  const auto args = split(arguments, ';');
  if (args.size() != 2 || config < 0 || config > 15)
    throw std::runtime_error("invalid scenario/configuration");
  const auto id = integer(args[0]), variant = integer(args[1]);
  if (id < 0 || id >= 1152 || variant < 0 || variant > 1 || id / 72 != config ||
      (id / 36) % 2 != variant || names[std::size_t(id % 36)] != name)
    throw std::runtime_error("inconsistent scenario identity");
  R::ScopedFpu context(
      R::Fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1));
  Replay replay(name, unsigned(variant));
  replay.recipe();
  return {"ok", replay.snapshot()};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x98915b58d742cb2dULL;
constexpr std::size_t expected_rows = 1152;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: combat-creature-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 1024ULL * 1024 * 1024;
  constexpr std::size_t max_line = 16 * 1024 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# combat_creature_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "combat-creature mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error("incomplete or changed combat-creature corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact combat-creature vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
