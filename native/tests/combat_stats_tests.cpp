#include "champions/combat_stats.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
using namespace champions::simulation::rta;
namespace c = champions::simulation::rta::combat;
namespace {
std::string join(const std::vector<std::string> &v, const char *sep) {
  std::string o;
  bool first = true;
  for (const auto &x : v) {
    if (!first)
      o += sep;
    first = false;
    o += x;
  }
  return o;
}
std::string text(const c::Text &s) {
  std::vector<std::string> v;
  for (auto cp : s)
    v.push_back(std::to_string(static_cast<std::uint32_t>(cp)));
  return v.empty() ? "-" : join(v, ".");
}

std::string sc(const c::Text &s) { return "s" + text(s); }
std::string sc(const std::string &s) {
  c::Text u;
  for (unsigned char x : s)
    u.push_back(x);
  return sc(u);
}
template <class T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
std::string sc(T v) {
  return std::to_string(v);
}
std::string sc(const c::EventValue &);
template <class A, class B> std::string sc(const std::pair<A, B> &);
template <class T> std::string sc(const std::vector<T> &v) {
  std::vector<std::string> s;
  for (const auto &x : v)
    s.push_back(sc(x));
  return "[" + join(s, ",") + "]";
}
template <class A, class B> std::string sc(const std::pair<A, B> &v) {
  return "[" + sc(v.first) + "," + sc(v.second) + "]";
}
template <class T, std::size_t N> std::string sc(const std::array<T, N> &v) {
  return sc(std::vector<T>(v.begin(), v.end()));
}
std::string sc(const c::EventValue &v) {
  return std::visit(
      [](const auto &x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same<T, std::monostate>::value)
          return "_";
        else
          return sc(x);
      },
      v.value);
}
std::string rawopt(const std::optional<Raw> &v) {
  return v ? std::to_string(*v) : "_";
}
c::Text tag(int i) { return U"p" + c::Text(1, char32_t('0' + i)); }
struct Fixture {
  c::Tables t;
  c::World w;
  std::array<c::ItemDef, 20> defs;
  std::array<c::Item, 20> items;
  std::array<c::Obj, 4> objs;
  std::array<c::Spell, 3> spells;
  int n;
  explicit Fixture(int v)
      : w(std::uint32_t(v) * 0x1234567u, std::uint32_t(v) * 0x7654321u), n(v) {
    for (int i = 0; i < 81; ++i)
      t.exp_table[i] = i * i * 100;
    for (int i = 0; i < 7; ++i) {
      std::array<int, 6> a{5 + i, 2 + i, 3, 7 + i, 4, 6};
      for (int j = 0; j < 6; ++j)
        t.class_formulas[i][j] = c::itof(a[j]);
      t.passive_masks[i][0] = 0x40;
      t.passive_masks[i][1] = 0x40000;
      t.passive_masks[i][2] = 0x40;
    }
    for (int j = 0; j < 6; ++j)
      t.class_default[j] = c::itof(j + 1);
    t.figurine_stats = {U"str", U"int", U"dex", U"sta"};
    t.figurine_tiers = {U"tiny", U"small", U"big", U"huge"};
    t.boss_names = {U"boss", U"ος", U"i\u0307", U"абв", U"𐐨", U"aς\u0301"};
    t.boss_gate = U"gate";
    t.boss_pairs = {{U"paired", U"wanted"}};
    t.boss_last = {U"lasta", U"lastb"};
    t.no_scale_name = U"NO_SCALE";
    t.fixed_levels = {U"fixed1", U"fixed2"};
    w.num_players = n % 4;
    w.monster_level = c::itof(1 + n % 80);
    w.difficulty = n % 7;
    w.level_name = std::array<c::Text, 4>{U"field", U"FIXED1", U"fixed1",
                                          U"fixed2"}[n % 4];
    w.medal_mission = (n / 4) % 2;
    w.load_cheat = (n / 8) % 2;
    w.frame = 50 + n % 80;
    w.pvp_a = (n / 2) % 2;
    const std::array<int, 20> codes{0x23, 0xd, 0x15, 0x16, 0x18, 0x17, 0x26,
                                    0,    1,   2,    3,    4,    5,    6,
                                    7,    8,   9,    10,   0x1e, 0x2a};
    for (int i = 0; i < 20; ++i) {
      auto &d = defs[i];
      d.name = std::array<c::Text, 5>{U"strtiny", U"intsmall", U"dexbig",
                                      U"stahuge", U"strnone"}[i % 5];
      d.dmg_min = (n * 7 + i * 3) % 80 - 20;
      d.dmg_max = (n * 11 + i * 2) % 120 - 10;
      d.armor = (n * 3 + i) % 90 - 15;
      d.attack = (n * 13 + i) % 150 - 30;
      d.category = i % 3;
      d.subtype = (n + i) % 7;
      d.dmg_type = std::array<unsigned, 4>{1, 4, 8, 2}[(n + i) % 4];
      d.flags = i >= 17 ? UINT64_C(1) << 45 : 0;
      auto &item = items[i];
      item.defn = &d;
      item.count = (n + i) % 7 - 2;
      item.flags = (n % 5 == 0 ? UINT64_C(1) << 36 : 0) |
                   (n % 7 == 0 ? UINT64_C(1) << 43 : 0);
      for (int j = 0; j < 4; ++j)
        item.mods[j] = {codes[(n + i + j * 3) % 20],
                        (n * 17 + i * 5 + j) % 161 - 40,
                        (n * 19 + i * 7 + j) % 171 - 20, 0};
    }
    for (int i = 0; i < 4; ++i) {
      auto &o = objs[i];
      o.tag = tag(i);
      o.flags = 0x60;
      o.slot = i;
      o.pos = {c::itof(i * 10), c::itof(i * 5), 0};
      o.state = n % 3 ? 1 : 0;
      o.attack_start = w.frame - n % 100;
      o.attack_chain = w.frame - n % 40;
      o.counter2 = (n / 4) % 2;
      o.counter4 = (n / 8) % 2;
      o.field_dc = c::itof(2);
      c::PlayerRec p;
      p.cls = (n + i) % 7;
      p.level58 = 1 + (n * 3 + i) % 79;
      p.level59 = 1 + (n + i) % 79;
      for (int j = 0; j < 4; ++j) {
        p.base[j] = (n * 7 + i * 11 + j * 17) % 200 - 30;
        p.eff[j] = (n * 3 + i * 9 + j * 7) % 150 - 20;
      }
      p.hp = c::itof((n + i) % 5 == 0 ? 0 : 20);
      p.max_hp = c::itof(100);
      p.mana = c::itof(10);
      p.max_mana = c::itof(70);
      p.skill_points = n;
      p.stat_points = n % 128;
      p.passives[0] = n % 140;
      p.passives[1] = (n * 3) % 140;
      p.passives[2] = 77;
      p.obj = (n + i) % 11 ? &o : nullptr;
      for (int j = 0; j < 17; ++j)
        p.equip[j] = (n + i + j) % 3 ? &items[j] : nullptr;
      p.weapon_sets = {&items[0], &items[1]};
      p.active_set = n % 9 == 0 ? -1 : n % 2;
      p.second_weapon = n % 2 ? &items[1] : nullptr;
      p.bag2 = {nullptr, &items[17], &items[18], &items[19]};
      for (int s : {0x3f, 0x3b, 0x3c, 0x3d, 0x30, 0x4f, 0x5c, 0x47})
        p.skills.emplace_back(s, 1 + (n + s) % 9);
      p.skills.emplace_back(-1, 0);
      p.skills.emplace_back(0x47, 99);
      w.players.push_back(p);
    }
    spells[0].kind = U"AncestralCall";
    spells[0].skill = 77;
    spells[0].level = 1 + n % 5;
    spells[0].owner = &objs[1];
    spells[1].kind = U"BloodLust";
    spells[1].skill = 78;
    spells[1].level = 2 + n % 7;
    spells[1].owner = &objs[0];
    spells[1].f2c = n % 2;
    spells[2].kind = U"GiantGrowth";
    spells[2].skill = 79;
    spells[2].level = 3;
    spells[2].owner = &objs[2];
    objs[1].spells = {&spells[0]};
    objs[0].spells = {&spells[1], &spells[2]};
    if (n % 3 && w.players[1].obj)
      w.alive = {1};
    w.stat_fn = [this](auto skill, auto stat, auto level) {
      w.log("stat", skill, stat, level);
      if (n % 29 == 0) {
        w.seed.draw();
        w.players[0].mana = c::fadd(w.players[0].mana, ONE);
      }
      if (n % 97 == 0)
        throw c::Error("authored callback");
      return stat == 0 ? 100 + (level * 7) % 90 : 10 + (level * 3) % 20;
    };
    w.adjust_fn = [this](auto skill, auto level, auto cap) {
      w.log("adjust", skill, level, cap);
      return std::min(level, cap);
    };
    w.pool_fn = [this](auto size) {
      w.log("pool_callback", size);
      return n % 2 == 0;
    };
    w.enemy_fn = [this](auto *a, auto *b) {
      w.log("enemy", a ? c::EventValue(a->tag) : c::EventValue(nullptr),
            b ? c::EventValue(b->tag) : c::EventValue(nullptr));
      return n % 5 != 0;
    };
  }
  std::string objlink(c::Obj *p) { return p ? sc(U"@" + p->tag) : "_"; }
  std::string itemlink(c::Item *p) {
    return p ? sc("item" + std::to_string(p - items.data())) : "_";
  }
  std::string itemlist(const std::vector<c::Item *> &v) {
    std::vector<std::string> s;
    for (auto *p : v)
      s.push_back(itemlink(p));
    return "[" + join(s, ",") + "]";
  }
  std::string spellrec(const c::Spell &p) {
    return join({sc(p.kind), sc(p.skill), sc(p.level), objlink(p.owner),
                 sc(p.f18), sc(p.f1c), sc(p.f20), sc(p.f2c)},
                "|");
  }
  std::string objrec(const c::Obj &o) {
    std::vector<std::string> sp;
    for (auto *p : o.spells)
      sp.push_back(spellrec(*p));
    return join({sc(o.tag),
                 sc(o.flags),
                 sc(o.pos),
                 sc(o.facing),
                 sc(o.owner_link),
                 objlink(o.owner),
                 sc(o.cls),
                 sc(o.innoruuk_immune),
                 sc(o.res),
                 sc(o.flash),
                 sc(o.cflags),
                 sc(o.kind),
                 sc(o.dot),
                 sc(o.field_dc),
                 sc(o.armor),
                 sc(o.stamp_f0),
                 sc(o.hp),
                 sc(o.max_hp),
                 sc(o.status),
                 sc(o.poison_a),
                 sc(o.poison_b),
                 objlink(o.status_source),
                 sc(sp),
                 sc(o.counter2),
                 sc(o.counter4),
                 sc(o.counter11),
                 sc(o.counter12),
                 sc(o.slot),
                 sc(o.state),
                 sc(o.attack_start),
                 sc(o.attack_chain),
                 sc(o.bonus),
                 sc(o.model_points),
                 sc(o.boss_rank),
                 sc(o.exp_value),
                 sc(o.own_seed),
                 objlink(o.last_attacker)},
                "|");
  }
  std::string playerrec(const c::PlayerRec &p) {
    return join({sc(p.cls),
                 sc(p.level58),
                 sc(p.level59),
                 sc(p.exp),
                 sc(p.base),
                 sc(p.eff),
                 sc(p.hp),
                 sc(p.max_hp),
                 sc(p.mana),
                 sc(p.max_mana),
                 sc(p.attack),
                 sc(p.dmg_min),
                 sc(p.dmg_max),
                 sc(p.stagger),
                 sc(p.skill_points),
                 sc(p.stat_points),
                 sc(p.passives),
                 sc(p.load_limit),
                 itemlist(p.bag2),
                 objlink(p.obj),
                 itemlist(p.equip),
                 itemlist(p.weapon_sets),
                 itemlink(p.second_weapon),
                 sc(p.active_set),
                 sc(p.skills)},
                "|");
  }
  std::string state() {
    std::vector<std::string> out;
    out.push_back("[" + sc(w.seed.state) + "," + sc(w.seed.draws) + "," +
                  sc(w.cosmetic.state) + "," + sc(w.cosmetic.draws) + "]");
    out.push_back(sc(w.events));
    out.push_back(
        "[" +
        join({sc(w.frame), sc(w.pvp_a), sc(w.cutscene), sc(w.numbers_option),
              sc(w.network_mode), sc(w.monster_level), sc(w.focus), sc(w.music),
              sc(w.music_index), sc(w.num_players), sc(w.alive),
              sc(w.medal_mission), sc(w.difficulty), sc(w.level_name),
              sc(w.load_cheat)},
             ",") +
        "]");
    for (const auto &p : w.players)
      out.push_back(playerrec(p));
    for (const auto &o : objs)
      out.push_back(objrec(o));
    return join(out, ";");
  }
};
std::pair<std::string, std::string> run(const std::string &op,
                                        const std::vector<std::int64_t> &a) {
  int mode = int(a.at(0)), n = int(a.at(1));
  auto &context =
      set_fpu(static_cast<Fpu::Model>(mode / 4), mode & 2, mode & 1);
  Fixture f(n);
  auto &t = f.t;
  auto &w = f.w;
  auto &items = f.items;
  auto &objs = f.objs;
  std::string result = "-";
  auto arg = [&](std::size_t i) { return a.at(i + 2); };
  try {
    if (op == "lower") {
      c::Text in;
      for (std::size_t i = 2; i < a.size(); ++i)
        in.push_back(char32_t(a[i]));
      result = text(c::unicode_lower(in));
    } else if (op == "boss") {
      const std::array<c::Text, 12> names{
          U"other", U"BOSS", U"gAtE", U"paired", U"LaStA",    U"lastB",
          U"ΟΣ",    U"İ",    U"АБВ",  U"𐐀",      U"AΣ\u0301", U"AΣA"};
      result = sc(c::boss_rank(
          t, n % 5 != 0,
          n % 13 == 0 ? std::nullopt : std::optional<c::Text>(names[n % 12]),
          n % 2 ? U"WANTED" : U"", n));
    } else if (op == "level")
      result = sc(
          c::level_scale(t, int(arg(0)), n % 7, n % 2,
                         std::array<c::Text, 4>{U"other", U"fixed1", U"fixed2",
                                                U"FIXED1"}[n % 4]));
    else if (op == "monster") {
      std::array<Raw, 5> p{};
      for (int i = 0; i < 5; ++i)
        p[i] = c::itof(1 + (n + i) % 3);
      auto v = c::monster_stats(
          t, w, p, n % 3,
          n % 3 == 0
              ? std::nullopt
              : std::optional<c::Text>(n % 3 == 1 ? U"other" : U"no_scale"),
          n % 2, std::uint32_t(arg(0)), n % 7 == 0);
      result = "[" +
               join({rawopt(v.hp), rawopt(v.max_hp), sc(v.armor), sc(v.dmg_low),
                     sc(v.dmg_high), sc(v.attack), sc(v.exp_value)},
                    ",") +
               "]";
    } else if (op == "monster_raw") {
      w.monster_level = Raw(arg(0));
      std::array<Raw, 5> p{};
      for (int i = 0; i < 5; ++i)
        p[i] = Raw(arg(i + 1));
      auto v =
          c::monster_stats(t, w, p, n % 3, U"other", true, 0xffffffff, n % 2);
      result = "[" +
               join({rawopt(v.hp), rawopt(v.max_hp), sc(v.armor), sc(v.dmg_low),
                     sc(v.dmg_high), sc(v.attack), sc(v.exp_value)},
                    ",") +
               "]";
    } else if (op == "passive")
      result = sc(c::passive_level(t, w.players[0], int(arg(0)),
                                   std::uint64_t(arg(1))));
    else if (op == "cap")
      result = sc(c::exp_cap(t, int(arg(0))));
    else if (op == "for_exp")
      result = sc(c::level_for_exp(t, arg(0)));
    else if (op == "item") {
      auto range = c::item_damage_range(items[0], n % 2);
      result = "[" +
               join({sc(c::item_armor_value(items[0])),
                     sc(c::item_attack_rating(items[0])), sc(range.first),
                     sc(range.second)},
                    ",") +
               "]";
    } else if (op == "armor")
      result = sc(c::recalc_armor(t, w, int(arg(0))));
    else if (op == "stats")
      c::recalc_stats(t, w);
    else if (op == "damage")
      result = sc(c::recalc_damage(t, w, objs[0],
                                   n % 8 == 0 ? nullptr : &items[0], n % 2));
    else if (op == "give") {
      auto &p = w.players[0];
      p.exp = c::at(t.exp_table, p.level59 - 1);
      p.hp = n % 5 == 0 ? 0 : c::itof(20);
      result = sc(c::give_exp(t, w, int(arg(0)), int(arg(1)), [&](int slot) {
        w.log("callback_level", slot);
        w.cosmetic.draw();
        if (n % 17 == 0)
          w.players[slot].mana = c::fadd(w.players[slot].mana, ONE);
        if (n % 31 == 0)
          throw c::Error("callback level");
      }));
    } else if (op == "kill") {
      c::Obj creature;
      creature.tag = U"monster";
      creature.exp_value = c::itof(arg(0));
      creature.max_hp = c::itof(900);
      result = sc(c::kill_experience(t, w, creature, int(arg(1)),
                                     std::uint32_t(arg(2))));
    } else if (op == "edge") {
      int k = int(arg(0));
      auto &p = w.players[0];
      p.obj = &objs[0];
      w.num_players = 1;
      if (k == 0) {
        p.eff = {INT64_MAX, INT64_MIN, INT64_MIN, INT64_MAX};
        result = sc(c::recalc_armor(t, w, 0));
      } else if (k == 1) {
        p.exp = INT64_MAX;
        p.stat_points = INT64_MAX;
        result = sc(c::give_exp(t, w, 0, 2147483647));
      } else if (k == 2) {
        t.exp_table[0] = INT64_MIN;
        t.exp_table[1] = INT64_MAX;
        result = sc(c::exp_cap(t, 0));
      } else if (k == 3) {
        t.exp_table[80] = INT64_MAX;
        result = sc(c::exp_cap(t, 80));
      } else if (k == 4)
        result = sc(c::passive_level(t, p, INT64_C(1) << 32, 0x40));
      else if (k == 5) {
        p.base = {INT64_MAX, INT64_MIN, (INT64_C(1) << 40) + 7,
                  -(INT64_C(1) << 40) - 9};
        c::recalc_stats(t, w);
      } else if (k == 6 || k == 7) {
        items[0].defn->subtype = k == 6 ? 4 : 6;
        p.active_set = 0;
        result = sc(c::recalc_armor(t, w, 0));
      } else if (k >= 8 && k <= 10) {
        w.alive = {1};
        w.players[1].obj = &objs[1];
        auto *spell = objs[1].spells[0];
        if (k == 8)
          spell->owner = &objs[0];
        if (k == 9)
          objs[0].pos = {c::itof(20000), c::itof(30000), 0};
        if (k == 10) {
          spell->owner = nullptr;
          w.pvp_a = 0;
        }
        result = sc(c::recalc_damage(t, w, objs[0], &items[0], true));
      } else if (k >= 11 && k <= 14) {
        w.alive.clear();
        p.cls = 5;
        objs[0].state = 1;
        p.hp = c::itof(k == 11 ? 90 : 20);
        p.max_hp = c::itof(100);
        if (k == 12)
          objs[0].attack_chain = w.frame - 27;
        if (k == 13)
          objs[0].attack_start = w.frame + 60;
        if (k == 14) {
          objs[0].spells.clear();
          p.skills = {{0x47, 0}};
        }
        result = sc(c::recalc_damage(t, w, objs[0], &items[0], true));
      } else if (k == 15 || k == 16) {
        w.alive.clear();
        p.cls = 6;
        objs[0].state = 1;
        objs[2].field_dc = k == 15 ? HALF : c::itof(5);
        result = sc(c::recalc_damage(t, w, objs[0], &items[0], true));
      } else if (k == 17 || k == 18) {
        w.alive.clear();
        p.cls = 4;
        items[0].defn->subtype = 0;
        items[0].defn->dmg_type = 8;
        items[0].flags = k == 17 ? 0 : UINT64_C(1) << 36;
        result = sc(c::recalc_damage(t, w, objs[0], &items[0], true));
      } else if (k == 19) {
        p.level59 = 1;
        p.exp = 0;
        result = sc(c::give_exp(t, w, 0, 500, [&](auto) {
          w.players.emplace_back();
          w.log("append", w.players.size());
        }));
      } else if (k == 20) {
        items[0].defn->armor = -32768;
        items[0].defn->attack = 32767;
        items[0].defn->dmg_min = -32768;
        items[0].defn->dmg_max = 32767;
        items[0].mods = {{0x23, -32768, 0, 0},
                         {0xd, 32767, 0, 0},
                         {0x15, -123, 234, 0},
                         {0x16, 0, 32767, 0}};
        auto range = c::item_damage_range(items[0], true);
        result = "[" +
                 join({sc(c::item_armor_value(items[0])),
                       sc(c::item_attack_rating(items[0])), sc(range.first),
                       sc(range.second)},
                      ",") +
                 "]";
      } else if (k == 21) {
        t.exp_table[80] = INT64_MIN;
        p.exp = 100;
        p.level59 = 80;
        result = sc(c::give_exp(t, w, 0, 1));
      } else if (k == 22) {
        w.stat_fn = [&](auto skill, auto stat, auto level) {
          w.log("stat_zero", skill, stat, level);
          return 0;
        };
        c::Obj creature;
        creature.tag = U"monster";
        creature.exp_value = c::itof(300);
        creature.max_hp = c::itof(20);
        result = sc(c::kill_experience(t, w, creature, 0, 0));
      } else if (k == 23) {
        w.num_players = 3;
        w.medal_mission = 0;
        w.level_name = U"field";
        w.monster_level = c::itof(9);
        auto v = c::monster_stats(t, w, {ONE, ONE, ONE, ONE, ONE}, 0,
                                  U"no_scale", true, n, false);
        result =
            "[" +
            join({rawopt(v.hp), rawopt(v.max_hp), sc(v.armor), sc(v.dmg_low),
                  sc(v.dmg_high), sc(v.attack), sc(v.exp_value)},
                 ",") +
            "]";
      } else if (k == 24) {
        w.alive.clear();
        p.cls = 0;
        items[0].defn->subtype = 4;
        w.stat_fn = [&](auto skill, auto stat, auto level) {
          w.log("changing_stat", skill, stat, level);
          if (skill == 0x30)
            items[0].defn->subtype = 6;
          if (skill == 0x4f) {
            items[0].defn->subtype = 0;
            items[0].defn->dmg_type = 8;
            p.cls = 4;
          }
          return 25;
        };
        result = sc(c::recalc_damage(t, w, objs[0], &items[0], true));
      } else if (k == 25) {
        w.alive = {1};
        w.players[1].obj = &objs[1];
        w.players[2].obj = &objs[2];
        w.stat_fn = [&](auto skill, auto stat, auto level) {
          w.log("append_alive", skill, stat, level);
          if (skill == 77 && stat == 3) {
            auto *more = objs[0].spells[1];
            more->kind = U"AncestralCall";
            more->skill = 88;
            more->owner = &objs[0];
            objs[2].spells = {more};
            w.alive.push_back(2);
          }
          return skill == 88 ? 200 : 100;
        };
        result = sc(c::recalc_damage(t, w, objs[0], nullptr, true));
      } else if (k == 26 || k == 27) {
        p.cls = k == 26 ? 8 : -1;
        objs[0].slot = k == 26 ? 5 : 0;
        c::recalc_stats(t, w);
      } else if (k == 28) {
        w.num_players = 3;
        w.stat_fn = [&](auto skill, auto stat, auto level) {
          w.log("changing_count", skill, stat, level);
          w.num_players = 0;
          return 15;
        };
        c::recalc_stats(t, w);
      } else
        throw std::runtime_error("bad edge case");
    } else if (op == "error") {
      switch (arg(0)) {
      case 0:
        t.fixed_levels.clear();
        result = sc(c::level_scale(t, 3, 1, false, U"x"));
        break;
      case 1:
        t.boss_last.clear();
        result = sc(c::boss_rank(t, true, U"absent", U"", 0));
        break;
      case 2:
        w.players[0].cls = -8;
        result = sc(c::passive_level(t, w.players[0], 0, 0x40));
        break;
      case 3:
        w.players[0].passives.clear();
        result = sc(c::passive_level(t, w.players[0], 0, 0x40));
        break;
      case 4:
        t.exp_table.clear();
        result = sc(c::exp_cap(t, 3));
        break;
      case 5:
        items[0].defn = nullptr;
        result = sc(c::item_armor_value(items[0]));
        break;
      case 6:
        w.players[0].equip.clear();
        result = sc(c::recalc_armor(t, w, 0));
        break;
      case 7:
        w.players[0].eff.clear();
        result = sc(c::recalc_damage(t, w, objs[0], nullptr, false));
        break;
      case 8:
        w.players[0].cls = -1;
        result = sc(c::passive_level(t, w.players[0], 0, 0x40));
        break;
      case 9:
        w.players[0].active_set = -3;
        result = sc(c::recalc_armor(t, w, 0));
        break;
      case 10:
        w.alive = {-1};
        w.players.back().obj = nullptr;
        result = sc(c::recalc_damage(t, w, objs[0], nullptr, false));
        break;
      case 11:
        w.players[0].base.clear();
        w.num_players = 1;
        c::recalc_stats(t, w);
        break;
      default:
        throw std::runtime_error("unknown error case");
      }
    } else
      throw std::runtime_error("unknown operation");
  } catch (const c::Error &) {
    result = "!Error";
  }
  if (context.model != static_cast<Fpu::Model>(mode / 4) ||
      context.mul_bias != bool(mode & 2) ||
      context.div_nearest != bool(mode & 1))
    throw std::runtime_error("arithmetic context changed");
  return {result, op == "lower" ? "-" : f.state()};
}
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string p;
  while (std::getline(ss, p, sep))
    out.push_back(p);
  return out;
}
} // namespace
void invariants() {
  c::Tables t;
  for (int i = 0; i < 81; ++i)
    t.exp_table[i] = i * 100;
  c::World w;
  w.players.emplace_back();
  w.players[0].skill_points = INT64_MAX;
  bool rejected = false;
  try {
    c::give_exp(t, w, 0, 100);
  } catch (const c::Unsupported &) {
    rejected = true;
  }
  if (!rejected || w.players[0].skill_points != INT64_MAX ||
      w.players[0].level59 != 1 || w.players[0].exp != 100)
    throw std::runtime_error("skill point bound/state");
  w.players[0].skill_points = 0;
  w.players[0].exp = 0;
  int callbacks = 0;
  rejected = false;
  try {
    c::give_exp(t, w, 0, 100, [&](auto) {
      ++callbacks;
      w.players[0].level59 = 1;
    });
  } catch (const c::Unsupported &) {
    rejected = true;
  }
  if (!rejected || callbacks != c::MAX_LEVEL_TRANSITIONS ||
      w.players[0].skill_points != c::MAX_LEVEL_TRANSITIONS ||
      w.players[0].level59 != 1)
    throw std::runtime_error("level loop budget/state");
  rejected = false;
  try {
    c::unicode_lower(c::Text(1, char32_t(0x110000)));
  } catch (const c::Error &) {
    rejected = true;
  }
  if (!rejected)
    throw std::runtime_error("invalid Unicode codepoint");
}
int main(int argc, char **argv) {
  try {
    invariants();
    if (argc < 2 || argc > 3)
      throw std::runtime_error("usage: combat-stats-tests TSV [TRACE]");
    std::ifstream input(argv[1]);
    if (!input)
      throw std::runtime_error("open fixture");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary | std::ios::trunc);
      if (!trace)
        throw std::runtime_error("open trace");
    }
    std::string line;
    std::size_t count = 0;
    std::map<std::string, std::size_t> counts;
    std::uint64_t hash = 14695981039346656037ULL;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      auto c = split(line, '\t');
      if (c.size() != 5 || c[0] != "combat_stats")
        throw std::runtime_error("bad fixture schema");
      std::vector<std::int64_t> a;
      for (const auto &p : split(c[2], ','))
        a.push_back(std::stoll(p));
      auto actual = run(c[1], a);
      ++count;
      ++counts[c[1]];
      if (actual.first != c[3] || actual.second != c[4]) {
        std::cerr << "case " << count << " " << c[1] << " " << c[2]
                  << "\nexpected " << c[3] << "\nactual " << actual.first
                  << "\n";
        std::ofstream("work/combat-stats/expected-state.txt") << c[4];
        std::ofstream("work/combat-stats/actual-state.txt") << actual.second;
        throw std::runtime_error("reference mismatch");
      }
      std::string normalized = c[0] + "\t" + c[1] + "\t" + c[2] + "\t" +
                               actual.first + "\t" + actual.second + "\n";
      for (unsigned char ch : normalized) {
        hash ^= ch;
        hash *= 1099511628211ULL;
      }
      if (trace)
        trace << normalized;
    }
    if (!input.eof())
      throw std::runtime_error("read failed");
    // Final per-operation counts are pinned with the authored corpus below.
    for (const auto &op :
         {"boss", "level", "monster", "passive", "cap", "for_exp", "item",
          "armor", "stats", "damage", "give", "kill"})
      if (counts[op] != 2560)
        throw std::runtime_error("incomplete corpus");
    if (counts["error"] != 192 || counts["lower"] != 6397 ||
        counts["edge"] != 1392 || counts["monster_raw"] != 128 ||
        counts.size() != 16)
      throw std::runtime_error("incomplete corpus");
    if (trace) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace write failed");
    }
    std::cout << count
              << " exact combat stats traces passed; fnv1a64=" << std::hex
              << hash << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
