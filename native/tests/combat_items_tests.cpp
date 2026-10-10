#include "champions/combat_items.hpp"
#include "champions/combat_stats.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace c = champions::simulation::rta::combat;
namespace it = champions::simulation::rta::items;
namespace r = champions::simulation::rta;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> out;
  std::istringstream stream(s);
  std::string part;
  while (std::getline(stream, part, separator))
    out.push_back(part);
  return out;
}
std::string name_state(const c::Text &text) {
  if (text.empty())
    return "-";
  std::ostringstream out;
  out << std::hex;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i)
      out << ',';
    out << static_cast<std::uint32_t>(text[i]);
  }
  return out.str();
}
std::string item_state(const c::Item &item) {
  std::ostringstream out;
  if (item.defn) {
    const auto &d = *item.defn;
    out << d.dmg_min << ',' << d.dmg_max << ',' << d.armor << ',' << d.attack
        << ',' << d.category << ',' << d.subtype << ',' << d.dmg_type << ','
        << d.flags << '|' << name_state(d.name);
  } else
    out << "none";
  out << '|' << item.count << ',' << item.flags << '|';
  for (std::size_t i = 0; i < item.mods.size(); ++i) {
    if (i)
      out << ';';
    for (std::size_t j = 0; j < item.mods[i].size(); ++j) {
      if (j)
        out << ',';
      out << item.mods[i][j];
    }
  }
  return out.str();
}
void anchors() {
  c::ItemArena arena;
  auto definition = std::make_shared<it::Definition>();
  definition->addr = 123;
  definition->armor = 30;
  definition->name = std::string("A\0\xff", 3);
  it::Item first(definition);
  first.set_modifier(0x0d, 10, -20, 30);
  auto a = arena.convert(first), b = arena.convert(first.copy());
  require(a.defn == b.defn && arena.definition_count() == 1,
          "definition identity lost");
  require(a.defn->name == c::Text({U'A', 0, 255}),
          "Latin1 conversion lost codepoint");
  auto independent = std::make_shared<it::Definition>(*definition);
  auto distinct = arena.convert(it::Item(independent));
  require(distinct.defn != a.defn && arena.definition_count() == 2,
          "same address merged distinct definitions");
  first.mods[0]->values[0] = 70;
  require(a.mod(0x0d) == 10 && b.mod(0x0d) == 10,
          "converted slots alias mutable source");
  a.mods[0][1] = 40;
  require(b.mod(0x0d) == 10, "converted slots alias another record");
  definition->armor = 60;
  arena.refresh_definitions();
  require(a.defn->armor == 60 && b.defn->armor == 60,
          "definition refresh lost aliases");
  definition.reset();
  first.definition.reset();
  require(a.defn->armor == 60, "arena failed to retain source lifetime");
  auto empty = arena.convert(it::Item{});
  require(!empty.defn && empty.count == 0 && empty.mods == c::EMPTY_DESC,
          "missing definition conversion");
}
int main(int argc, char **argv) {
  try {
    require(argc == 2 || argc == 3,
            "usage: combat-items-tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream input(argv[1]);
    require(bool(input), "cannot open vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      require(bool(trace), "cannot open trace");
    }
    c::ItemArena arena;
    std::string line;
    std::size_t rows = 0;
    require(bool(std::getline(input, line)) && line == "# rta-combat-items-v1",
            "wrong schema");
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      auto fields = split(line, '\t');
      require(fields.size() == 5 && fields[0] == "combat-items" &&
                  fields[1] == "convert",
              "bad record schema");
      const auto args = split(fields[2], '|');
      require(args.size() == 5, "bad args");
      const auto config = std::stoul(args[0]);
      require(config < 16, "bad config");
      auto &context = r::set_fpu(static_cast<r::Fpu::Model>(config / 4),
                                 (config & 2) != 0, (config & 1) != 0);
      const auto numbers = split(args[2], ',');
      require(numbers.size() == 10, "bad numeric fields");
      auto definition = std::make_shared<it::Definition>();
      definition->damage_min = static_cast<std::int32_t>(std::stol(numbers[0]));
      definition->damage_max = static_cast<std::int32_t>(std::stol(numbers[1]));
      definition->armor = static_cast<std::int32_t>(std::stol(numbers[2]));
      definition->attack = static_cast<std::int32_t>(std::stol(numbers[3]));
      definition->category = static_cast<std::int32_t>(std::stol(numbers[4]));
      definition->subtype = static_cast<std::int32_t>(std::stol(numbers[5]));
      definition->damage_bits =
          static_cast<std::uint32_t>(std::stoul(numbers[6]));
      definition->flags = std::stoull(numbers[7]);
      if (args[3] != "-") {
        require(args[3].size() % 2 == 0, "bad byte string");
        for (std::size_t i = 0; i < args[3].size(); i += 2)
          definition->name.push_back(
              static_cast<char>(std::stoul(args[3].substr(i, 2), nullptr, 16)));
      }
      it::Item source(std::stoul(args[1]) ? definition : nullptr);
      source.count = static_cast<std::int32_t>(std::stol(numbers[8]));
      source.flags = std::stoull(numbers[9]);
      const auto mods = split(args[4], ';');
      require(mods.size() == 4, "bad slots");
      for (std::size_t i = 0; i < 4; ++i)
        if (mods[i] != "-") {
          const auto slot = split(mods[i], ',');
          require(slot.size() == 4, "bad modifier");
          source.mods[i] =
              it::Modifier{static_cast<std::int32_t>(std::stol(slot[0])),
                           {static_cast<std::int16_t>(std::stol(slot[1])),
                            static_cast<std::int16_t>(std::stol(slot[2])),
                            static_cast<std::int16_t>(std::stol(slot[3]))}};
        }
      auto converted = arena.convert(source);
      std::string result;
      try {
        const auto armor = c::item_armor_value(converted);
        const auto attack = c::item_attack_rating(converted);
        const auto range = c::item_damage_range(converted, false),
                   elements = c::item_damage_range(converted, true);
        std::ostringstream out;
        out << armor << ',' << attack << ',' << range.first << ','
            << range.second << ',' << elements.first << ',' << elements.second;
        result = out.str();
      } catch (const c::Error &) {
        result = "error";
      }
      require(context.model == static_cast<r::Fpu::Model>(config / 4) &&
                  context.mul_bias == ((config & 2) != 0) &&
                  context.div_nearest == ((config & 1) != 0),
              "item-to-combat call changed selected FPU settings");
      const auto state = item_state(converted);
      require(result == fields[3] && state == fields[4],
              "item-to-combat exact mismatch");
      if (trace.is_open())
        trace << fields[0] << '\t' << fields[1] << '\t' << fields[2] << '\t'
              << result << '\t' << state << '\n';
      ++rows;
    }
    require(input.eof() && rows == 8192, "missing or truncated vector corpus");
    if (trace.is_open()) {
      trace.flush();
      require(bool(trace), "trace write failed");
    }
    std::cout << rows
              << " exact item-to-combat cases and 8 identity/lifetime anchors "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
