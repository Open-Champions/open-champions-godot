#include "champions/item_generation.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using namespace champions::simulation::rta;
namespace it = champions::simulation::rta::items;
namespace {
std::string join(const std::vector<std::string> &values,
                 const std::string &sep) {
  std::string out;
  for (const auto &v : values) {
    if (!out.empty())
      out += sep;
    out += v;
  }
  return out.empty() ? "-" : out;
}
std::string itemtext(const it::Item &item) {
  std::ostringstream out;
  for (auto b : item.pack())
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(b);
  return out.str();
}
std::string itemtext(const std::optional<it::Item> &item) {
  return item ? itemtext(*item) : "-";
}
std::optional<std::string> picture(std::int64_t code) {
  switch (code) {
  case 0:
    return std::nullopt;
  case 1:
    return "ore";
  case 2:
    return "absent";
  case 3:
    return "";
  default:
    throw std::runtime_error("bad picture code");
  }
}
it::Tables tables(int config) {
  it::Tables t;
  auto def = [&](std::string name, int category, int subtype = 0, int slot = 0,
                 std::uint64_t flags = 0, int level = 1, int price = 100,
                 bool preset = false) {
    auto d = std::make_shared<it::Definition>();
    auto &rows = preset ? t.presets : t.defs;
    d->index = static_cast<std::int32_t>(rows.size());
    d->addr = (preset ? it::A_PRESETS : it::A_DEFS) +
              64 * static_cast<std::uint32_t>(d->index);
    d->preset = preset;
    d->name = std::move(name);
    d->level = level;
    d->weight = ONE;
    d->count = 1;
    d->damage_min = 2;
    d->damage_max = 5;
    d->price = price;
    d->armor = 4;
    d->attack = 3;
    d->slot = slot;
    d->category = category;
    d->subtype = subtype;
    d->damage_bits = subtype == 3 ? 8 : 1;
    d->flags = flags;
    d->picture = "ore";
    d->model_row = 0;
    rows.push_back(d);
  };
  const std::array<int, 3> subtypes{6, 3, 0};
  for (unsigned i = 0; i < 3; ++i)
    def("weapon" + std::to_string(i), 0, subtypes[i], 0, (3u << 11) | 2, 1,
        100 + static_cast<int>(i) * 25);
  const std::array<int, 6> slots{11, 4, 9, 12, 3, 0};
  for (unsigned i = 0; i < 6; ++i) {
    def("armor" + std::to_string(i), 1, 0, slots[i], 2u << 11, 1,
        70 + static_cast<int>(i));
    def("cloth" + std::to_string(i), 1, 0, slots[i], (2u << 11) | 0x80000, 1,
        70 + static_cast<int>(i));
  }
  def("circlet_demo", 1, 0, 11, 0, 1, 75);
  def("other", 2, 0, 0, 3u << 11, 1, 55);
  def("ore_low", 2, 0, 0, (2u << 11) | UINT64_C(0x800000000), 1, 40);
  def("ore_high", 2, 0, 0, UINT64_C(0x800000000) | 0x10000, 38, 90);
  def("figure_low", 2, 0, 0, UINT64_C(0x200000000000), 1, 35);
  def("figure_high", 2, 0, 0, UINT64_C(0x200000000000) | (4u << 11), 38, 95);
  for (const auto &group : {"m", "h"})
    for (int i = 0; i < 12; ++i)
      def(std::string("p_") + group + std::to_string(i), 2, 0, 0, 0, 1, 10 + i);
  def("arrows", 0, 6, 0, 0, 1, 2);
  def("recall", 2, 0, 0, 0, 1, 30);
  for (unsigned i = 0; i < 12; ++i)
    def("preset" + std::to_string(i) + "+boost2", static_cast<int>(i % 3), 0, 0,
        (3u << 11) | (i == 4 ? 0x80000 : 0), i < 9 ? 1 : 40,
        100 + static_cast<int>(i / 3) * 20, true);
  const std::array<int, 4> plevels{1, 50, 1, 2}, slevels{1, 40, 2, 1};
  const std::array<unsigned, 4> masks{0, 1, 2, 0}, damage{0, 0, 8, 8};
  for (unsigned i = 0; i < 4; ++i) {
    it::Prefix p;
    p.index = static_cast<int>(i);
    p.key = "pre" + std::to_string(i);
    p.level = plevels[i];
    p.mask = masks[i];
    p.damage_mask = damage[i];
    p.texts.fill("+boost" + std::to_string(i + 1));
    if (i == 2)
      p.texts[1] = std::nullopt;
    t.prefixes.push_back(p);
    it::Suffix s;
    s.index = static_cast<int>(i);
    s.key = "suf" + std::to_string(i);
    s.level = slevels[i];
    s.texts.fill("+worth" + std::to_string(i * 5));
    if (i == 2)
      s.texts[1] = std::nullopt;
    t.suffixes.push_back(s);
  }
  t.formats = {{"boost%d", 0}, {"worth%d", 0x25}, {"guard%d", 0x0d}};
  t.potion_stem = "p_";
  t.thrown_stem = "thrown_";
  t.circlet_stem = "circlet";
  t.circlet_stem_equip = "circlet";
  t.arrows = "arrows";
  t.recall = "recall";
  t.name_key_format = "%s";
  t.name_space = " ";
  t.of_key = "of";
  t.gold_scale_multiplayer = UINT64_C(0x4003333333333333);
  t.weapon_multipliers.assign(8, ONE);
  for (int i = 0; i < 12; ++i) {
    t.quality_rows.push_back(
        i % 2 == 0 ? std::array<Raw, 3>{0x3e800000, 0x3f000000, 0x3e800000}
                   : std::array<Raw, 3>{0x3d800000, 0x3f400000, 0x3e400000});
    t.mana_potions.push_back("p_m" + std::to_string(i));
    t.heal_potions.push_back("p_h" + std::to_string(i));
  }
  t.mana_potions[10] = t.heal_potions[0];
  t.mana_potions[11] = t.heal_potions[1];
  if (config == 1) {
    t.defs.clear();
    t.presets.clear();
  }
  if (config == 2)
    t.prefixes.clear();
  if (config == 3)
    t.suffixes.clear();
  if (config == 4)
    for (auto &s : t.suffixes)
      s.level = 99;
  if (config == 5)
    t.defs[17]->picture = std::nullopt;
  if (config == 6) {
    for (auto &d : t.defs)
      d->flags |= 0x10000;
    for (auto &d : t.presets)
      d->flags |= 0x10000;
  }
  if (config == 7)
    t.suffixes[0].texts[2] = std::nullopt;
  if (config == 9) {
    for (auto &d : t.defs)
      d->flags &= ~UINT64_C(0xf800);
    for (auto &d : t.presets)
      d->flags &= ~UINT64_C(0xf800);
  }
  if (config == 10) {
    auto d = t.defs[2];
    t.defs = {d};
    t.presets.clear();
  }
  if (config == 11)
    t.quality_rows.clear();
  if (config == 12)
    for (auto &d : t.defs)
      if (d->category == 2 && !(d->flags & UINT64_C(0x200800000000)))
        d->name = "p_" + d->name;
  if (config == 13) {
    for (auto &d : t.defs)
      d->price = 0;
    for (auto &d : t.presets)
      d->price = 0;
  }
  if (config == 14)
    for (auto &d : t.defs)
      d->flags &= ~UINT64_C(0x200800000000);
  return t;
}
struct World : it::World {
  int mode;
  std::vector<std::string> events;
  explicit World(int m) : mode(m) {}
  void spawn_gold(std::int32_t amount, it::Rng &rng) override {
    events.push_back("gold:" + std::to_string(amount));
    if (mode == 1 || mode == 3 || mode == 4)
      events.push_back("ctor:" + std::to_string(rng.next()));
    it::World::spawn_gold(amount, rng);
  }
  bool spawn_item(const std::string &kind, it::Item &item,
                  it::Rng &rng) override {
    events.push_back("item:" + kind + ":" + itemtext(item));
    if (mode == 2 || (mode == 3 && kind == "equipment")) {
      events.push_back("fail:" + kind);
      return false;
    }
    if (mode == 1 || mode == 3 || mode == 4 || mode == 5)
      events.push_back("ctor:" + std::to_string(rng.next()));
    if (mode == 4) {
      item.count = 77;
      item.flags ^= 1;
      item.set_modifier(0x0e, 71);
    }
    if (mode == 5) {
      events.push_back("exception:item");
      throw it::Error("authored callback failure after draw");
    }
    return it::World::spawn_item(kind, item, rng);
  }
  void throw_item(const it::Item &item, std::int32_t angle) override {
    events.push_back("throw:" + std::to_string(angle));
    it::World::throw_item(item, angle);
  }
};
std::vector<std::string> split(const std::string &line, char delimiter) {
  std::vector<std::string> out;
  std::istringstream stream(line);
  std::string part;
  while (std::getline(stream, part, delimiter))
    out.push_back(part);
  return out;
}
std::pair<std::string, std::string> run(const std::string &op,
                                        const std::vector<std::int64_t> &a) {
  auto t = tables(static_cast<int>(a.at(0)));
  it::Rng rng(static_cast<Raw>(a.at(1)));
  it::Game g;
  g.num_players = static_cast<int>(a.at(2));
  g.difficulty = static_cast<int>(a.at(3));
  g.pvp_a = static_cast<int>(a.at(4));
  g.pvp_b = static_cast<int>(a.at(5));
  g.level = static_cast<Raw>(a.at(6));
  for (unsigned i = 0; i < 4; ++i)
    g.classes[i] = (a.at(7) & (1 << i)) ? 4 : 0;
  auto arg = [&](unsigned i) { return a.at(8 + i); };
  World world(op == "loot"                    ? static_cast<int>(arg(7))
              : op == "world"                 ? static_cast<int>(arg(0))
              : op == "named" && a.size() > 9 ? static_cast<int>(arg(1))
                                              : 0);
  std::vector<it::Item> stock;
  std::string result = "-";
  try {
    if (op == "preset")
      result = itemtext(it::generate_preset(t, g, rng, static_cast<int>(arg(0)),
                                            static_cast<int>(arg(1)),
                                            static_cast<int>(arg(2))));
    else if (op == "affixes")
      result = itemtext(it::generate_with_affixes(
          t, g, rng, static_cast<int>(arg(0)), static_cast<int>(arg(1)),
          static_cast<int>(arg(2)), static_cast<int>(arg(3))));
    else if (op == "reagent")
      result = itemtext(it::generate_reagent(
          t, g, rng, static_cast<int>(arg(0)), picture(arg(1))));
    else if (op == "figurine")
      result =
          itemtext(it::generate_figurine(t, g, rng, static_cast<int>(arg(0))));
    else if (op == "consumable")
      result = itemtext(it::generate_consumable(t, g, rng));
    else if (op == "shop") {
      std::vector<std::string> results;
      for (std::int64_t i = 0; i < arg(2); ++i)
        results.push_back(itemtext(
            it::shop_append_unique(t, g, rng, stock, static_cast<int>(arg(0)),
                                   static_cast<int>(arg(1)))));
      result = join(results, "~");
    } else if (op == "shop_buffer") {
      it::Rng probe = rng;
      auto first = it::generate_with_affixes(t, g, probe, 100000, 0, -1, 1);
      it::generate_with_affixes(t, g, probe, 100000, 0, -1, 1);
      if (first)
        stock.push_back(*first);
      result = itemtext(it::shop_append_unique(t, g, rng, stock, 100000, 0));
    } else if (op == "named")
      result = itemtext(it::spawn_named_item(
          t, rng, world,
          t.defs.at(static_cast<std::size_t>(arg(0)))->name + "+boost3"));
    else if (op == "classify") {
      const auto &d = *t.defs.at(static_cast<std::size_t>(arg(0)));
      result = std::to_string(it::weapon_group(d)) + "," +
               std::to_string(it::armor_group(d));
    } else if (op == "half")
      result = std::to_string(it::half_level(g));
    else if (op == "world") {
      auto item = it::parse_descriptor(t, t.defs[0]->name, -1, false, &rng);
      world.spawn_gold(-3, rng);
      const bool ok = world.spawn_item("named", item, rng);
      world.throw_item(item, -12);
      result = ok ? "1" : "0";
    } else if (op == "scatter") {
      unsigned calls = 0;
      const auto ground = [&](Raw x, Raw y, Raw z) {
        ++calls;
        world.events.push_back("terrain:" + std::to_string(x) + "," +
                               std::to_string(y) + "," + std::to_string(z));
        const auto mode = arg(3);
        if (mode == 0)
          return z;
        if (mode == 1)
          return calls == 10 ? z : it::fadd(z, 0x40c00000);
        if (mode == 2)
          return it::fadd(z, 0x40e00000);
        if (mode == 3)
          return it::fsub(z, 0x40bfffff);
        return calls > 2 ? it::fsub(z, ONE) : it::fsub(z, 0x40c00000);
      };
      const auto p =
          it::gold_scatter(rng, ground, static_cast<Raw>(arg(0)),
                           static_cast<Raw>(arg(1)), static_cast<Raw>(arg(2)));
      result = std::to_string(p[0]) + "," + std::to_string(p[1]) + "," +
               std::to_string(p[2]);
    } else if (op == "loot")
      result = it::loot_generate_drops(
          t, g, rng, world, static_cast<Raw>(arg(0)), static_cast<Raw>(arg(1)),
          static_cast<Raw>(arg(2)), static_cast<Raw>(arg(3)),
          static_cast<int>(arg(4)), picture(arg(5)), arg(6) != 0);
    else
      throw std::runtime_error("unknown operation");
  } catch (const it::Unsupported &) {
    result = "!Unsupported";
  } catch (const it::Error &) {
    result = "!Error";
  } catch (const std::out_of_range &) {
    result = "!Error";
  }
  std::vector<std::string> stock_text, drops;
  for (const auto &item : stock)
    stock_text.push_back(itemtext(item));
  for (const auto &drop : world.drops)
    drops.push_back(
        drop.kind + ":" +
        (drop.item ? itemtext(*drop.item) : std::to_string(drop.value)));
  const auto state = "seed=" + std::to_string(rng.seed) +
                     ";draws=" + std::to_string(rng.draws) +
                     ";stock=" + join(stock_text, "~") +
                     ";events=" + join(world.events, "|") +
                     ";drops=" + join(drops, "|");
  return {result, state};
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      throw std::runtime_error(
          "usage: item-generation-tests VECTORS.tsv [TRACE.tsv]");
    std::ifstream input(argv[1]);
    if (!input)
      throw std::runtime_error("cannot open vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw std::runtime_error("cannot open trace");
    }
    std::map<std::string, std::size_t> operations;
    std::size_t count = 0;
    std::uint64_t digest = UINT64_C(14695981039346656037);
    std::string line;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      const auto fields = split(line, '\t');
      if (fields.size() != 5 || fields[0] != "items")
        throw std::runtime_error("bad vector record");
      std::vector<std::int64_t> args;
      for (const auto &text : split(fields[2], ',')) {
        std::size_t end = 0;
        const auto v = std::stoll(text, &end);
        if (end != text.size())
          throw std::runtime_error("invalid argument");
        args.push_back(v);
      }
      // Every row must ignore the ambient core sensitivity setting.
      set_fpu(Fpu::Model::Nearest, true, true);
      const auto actual = run(fields[1], args);
      ++count;
      ++operations[fields[1]];
      if (actual.first != fields[3] || actual.second != fields[4]) {
        std::cerr << "row " << count << " " << fields[1]
                  << " args=" << fields[2] << "\nexpected " << fields[3]
                  << "\nactual " << actual.first << "\nexpected state "
                  << fields[4] << "\nactual state " << actual.second << '\n';
        throw std::runtime_error("exact item generation mismatch");
      }
      if (champions::simulation::rta::div(ONE, 0x40400000) != 0x3eaaaaab)
        throw std::runtime_error("item generation leaked FPU context");
      const auto record = fields[0] + "\t" + fields[1] + "\t" + fields[2] +
                          "\t" + actual.first + "\t" + actual.second + "\n";
      for (unsigned char byte : record) {
        digest ^= byte;
        digest *= UINT64_C(1099511628211);
      }
      if (trace.is_open()) {
        trace << record;
        if (!trace)
          throw std::runtime_error("trace write failed");
      }
    }
    const std::map<std::string, std::size_t> expected_operations{
        {"classify", 47},   {"half", 12},        {"preset", 243},
        {"affixes", 435},   {"shop", 153},       {"reagent", 2592},
        {"figurine", 216},  {"consumable", 256}, {"scatter", 256},
        {"named", 60},      {"world", 18},       {"loot", 2051},
        {"shop_buffer", 64}};
    if (input.bad() || operations != expected_operations)
      throw std::runtime_error(
          "missing/incomplete item generation operation coverage");
    if (trace.is_open()) {
      trace.flush();
      if (!trace)
        throw std::runtime_error("trace flush failed");
    }
    std::cout << count
              << " exact item generation traces passed; fnv1a64=" << std::hex
              << digest << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
