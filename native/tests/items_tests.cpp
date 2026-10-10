#include "champions/items.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace r = champions::simulation::rta;
namespace it = champions::simulation::rta::items;
namespace {
using Numbers = std::vector<std::uint64_t>;
using Bytes = std::vector<std::uint8_t>;
void require(bool value, const char *text) {
  if (!value)
    throw std::runtime_error(text);
}
std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto end = s.find(delim, start);
    out.push_back(s.substr(start, end - start));
    if (end == std::string::npos)
      return out;
    start = end + 1;
  }
}
std::string nums(const Numbers &n) {
  if (n.empty())
    return "-";
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < n.size(); ++i) {
    if (i)
      out << ',';
    out << std::setw(16) << n[i];
  }
  return out.str();
}
template <class T> std::string one(T n) {
  return nums({static_cast<std::uint64_t>(n)});
}
Numbers numbers(const std::string &s) {
  if (s == "-")
    return {};
  Numbers out;
  for (const auto &part : split(s, ',')) {
    require(part.size() == 16 &&
                part.find_first_not_of("0123456789abcdef") == std::string::npos,
            "invalid numeric field");
    out.push_back(std::stoull(part, nullptr, 16));
  }
  return out;
}
Bytes bytes(const std::string &s) {
  if (s == "-")
    return {};
  require(s.size() % 2 == 0 &&
              s.find_first_not_of("0123456789abcdef") == std::string::npos,
          "invalid byte field");
  Bytes out;
  for (std::size_t i = 0; i < s.size(); i += 2)
    out.push_back(
        static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return out;
}
std::string string(const std::string &s) {
  const auto data = bytes(s);
  return {data.begin(), data.end()};
}
template <class Container> std::string hex(const Container &data) {
  if (data.empty())
    return "-";
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto b : data)
    out << std::setw(2) << unsigned(b);
  return out.str();
}
std::int32_t si(std::uint64_t v) {
  return r::s32(static_cast<std::uint32_t>(v));
}
r::Raw u(std::uint64_t v) { return static_cast<std::uint32_t>(v); }
std::array<std::uint8_t, 72> stale(const Bytes &b) {
  require(b.size() == 72, "fixture stale size");
  std::array<std::uint8_t, 72> out{};
  std::copy(b.begin(), b.end(), out.begin());
  return out;
}
constexpr std::array<int, 43> codes{
    0x23, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x15, 0x16, 0x18, 0x17,
    0x14, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x0d, 0x0b, 0x0c, 5,
    6,    7,    8,    9,    10,   0,    1,    2,    3,    4,    0x21,
    0x1f, 0x20, 0x22, 0x24, 0x25, 0x27, 0x28, 0x2a, -1,   0x26};
it::Tables tables() {
  it::Tables t;
  auto row = [](unsigned i, bool preset) {
    auto d = std::make_shared<it::Definition>();
    d->addr = (preset ? it::A_PRESETS : it::A_DEFS) + 64 * i;
    d->index = static_cast<int>(i);
    d->preset = preset;
    d->name =
        preset ? "preset" + std::to_string(i) + "+m00" + std::to_string(i + 2)
               : "sample" + std::to_string(i);
    if (!preset && i == 3)
      d->name = "thrown-sample";
    if (!preset && i == 4)
      d->name = "band-sample";
    d->level = static_cast<int>(i + 1);
    d->weight = it::fadd(it::fint(i), 0x3fa00000);
    d->count = static_cast<int>(i + 1);
    d->damage_min = static_cast<int>(i + 2);
    d->damage_max = static_cast<int>(i + 7);
    d->armor = static_cast<int>(i * 3);
    d->attack = static_cast<int>(i * 5);
    d->slot = static_cast<int>(i);
    d->category = static_cast<int>(i % 3);
    d->subtype = static_cast<int>(i % 7);
    d->damage_bits = 1u << (i % 4);
    d->flags = ((i % 4) << 11) | (i == 4 ? 0x80000 : 0);
    d->model_row = static_cast<int>(i * 2);
    return d;
  };
  for (unsigned i = 0; i < 8; ++i)
    t.defs.push_back(row(i, false));
  for (unsigned i = 0; i < 3; ++i)
    t.presets.push_back(row(i, true));
  for (unsigned i = 0; i < 43; ++i) {
    std::ostringstream text;
    text << 'm' << std::setw(2) << std::setfill('0') << i;
    const unsigned count = codes[i] < 0 ? 1 : i % 3 + 1;
    for (unsigned k = 0; k < count; ++k) {
      if (k)
        text << ',';
      text << "%d";
    }
    t.formats.push_back(
        {text.str(),
         codes[i] < 0 ? std::nullopt : std::optional<std::int32_t>(codes[i])});
  }
  t.thrown_stem = "thrown";
  t.circlet_stem_equip = "band";
  t.potion_stem = "vial";
  for (unsigned i = 0; i < 7; ++i)
    t.weapon_multipliers.push_back(it::fdiv(it::fint(i + 2), 0x40800000));
  return t;
}
void override_definition(it::Definition &d, const Numbers &v) {
  require(v.size() == 14, "definition fixture size");
  d.level = si(v[0]);
  d.weight = u(v[1]);
  d.count = si(v[2]);
  d.damage_min = si(v[3]);
  d.damage_max = si(v[4]);
  d.price = si(v[5]);
  d.armor = si(v[6]);
  d.attack = si(v[7]);
  d.slot = si(v[8]);
  d.category = si(v[9]);
  d.subtype = si(v[10]);
  d.damage_bits = u(v[11]);
  d.flags = v[12];
  d.model_row = si(v[13]);
}
std::string execute(const std::string &op, const std::vector<std::string> &a,
                    it::Rng &rng, bool &has_rng) {
  auto n = numbers(a.at(0));
  if (op == "s8")
    return one(it::s8(u(n.at(0))));
  if (op == "s16")
    return one(it::s16(u(n.at(0))));
  if (op == "s32")
    return one(it::s32(u(n.at(0))));
  if (op == "fint")
    return one(it::fint(u(n.at(0))));
  if (op == "ftrunc")
    return one(it::ftrunc(u(n.at(0))));
  if (op == "fadd")
    return one(it::fadd(u(n.at(0)), u(n.at(1))));
  if (op == "fsub")
    return one(it::fsub(u(n.at(0)), u(n.at(1))));
  if (op == "fmul")
    return one(it::fmul(u(n.at(0)), u(n.at(1))));
  if (op == "fdiv")
    return one(it::fdiv(u(n.at(0)), u(n.at(1))));
  if (op == "flt")
    return one(it::flt(u(n.at(0)), u(n.at(1))));
  if (op == "fle")
    return one(it::fle(u(n.at(0)), u(n.at(1))));
  if (op == "fmax")
    return one(it::fmax(u(n.at(0)), u(n.at(1))));
  if (op == "powf")
    return one(it::powf(u(n.at(0)), u(n.at(1))));
  if (op == "float_times_double")
    return one(it::float_times_double(u(n.at(0)), n.at(1)));
  if (op == "rng") {
    has_rng = true;
    rng = it::Rng(u(n.at(0)));
    Numbers draws;
    for (std::uint64_t k = 0; k < n.at(1); ++k)
      draws.push_back(rng.next());
    return nums(draws);
  }
  if (op == "game") {
    it::Game g;
    g.num_players = si(n.at(0));
    g.classes = {si(n.at(1)), si(n.at(2)), si(n.at(3)), si(n.at(4))};
    g.difficulty = si(n.at(5));
    g.pvp_b = si(n.at(6));
    g.level = u(n.at(7));
    return nums(
        {g.level_times_0_4(), g.level_times_difficulty(), g.cloth_fraction()});
  }
  auto t = tables();
  if (op == "definition_index") {
    it::Item i(t.by_addr(u(n.at(0))));
    const auto index = it::encode_definition_index(t, i);
    const auto decoded = it::decode_definition_index(t, index);
    return nums(
        {static_cast<std::uint64_t>(index), decoded ? decoded->addr : 0});
  }
  if (op == "randomize_variant") {
    has_rng = true;
    rng = it::Rng(u(n.at(2)));
    it::Item i(t.by_addr(u(n.at(0))));
    i.flags = n.at(1);
    it::randomize_variant(i, rng);
    return nums({i.flags, i.variant()});
  }
  if (op == "load_resources") {
    has_rng = true;
    rng = it::Rng(u(n.at(0)));
    return one(it::load_resources(t, rng));
  }
  if (op == "required_level_for_value")
    return one(it::required_level_for_value(si(n.at(0))));
  if (op == "short_record")
    return hex(it::Item::unpack(t, bytes(a.at(0))).pack());
  // String-leading operations are dispatched before execute(), below.
  override_definition(*t.defs[0], n);
  const auto data = bytes(a.at(1));
  auto item = it::Item::unpack(t, data);
  if (op == "item_weight")
    return one(it::item_weight(item));
  if (op == "item_armor")
    return one(it::item_armor(item));
  if (op == "item_attack_rating")
    return one(it::item_attack_rating(item));
  if (op == "resource_index")
    return one(it::resource_index(item));
  if (op == "item_value")
    return one(it::item_value(t, item));
  if (op == "item_damage_range") {
    const auto result =
        it::item_damage_range(item, numbers(a.at(2)).at(0) != 0);
    return nums({static_cast<std::uint64_t>(result.first),
                 static_cast<std::uint64_t>(result.second)});
  }
  if (op == "record")
    return hex(item.pack(stale(data)));
  if (op == "undefined_offsets") {
    Numbers out;
    for (auto offset : item.undefined_offsets())
      out.push_back(offset);
    return nums(out);
  }
  if (op == "get_modifier") {
    n = numbers(a.at(2));
    return one(item.get(si(n.at(0)), static_cast<std::size_t>(n.at(1))));
  }
  if (op == "set_modifier") {
    n = numbers(a.at(2));
    const auto slot =
        item.set_modifier(si(n.at(0)), si(n.at(1)), si(n.at(2)), si(n.at(3)));
    Numbers out{static_cast<std::uint64_t>(slot)};
    for (auto b : item.pack(stale(data)))
      out.push_back(b);
    return nums(out);
  }
  if (op == "same_identity")
    return one(it::same_identity(item, it::Item::unpack(t, bytes(a.at(2)))));
  if (op == "copy_mutable") {
    const auto other = it::Item::unpack(t, bytes(a.at(2)));
    it::copy_mutable(item, other);
    return hex(item.pack(stale(data)));
  }
  if (op == "equip_restriction") {
    n = numbers(a.at(2));
    it::Player p;
    p.class_id = si(n.at(0));
    p.level = si(n.at(1));
    p.strength = si(n.at(2));
    p.intelligence = si(n.at(3));
    p.dexterity = si(n.at(4));
    p.stamina = si(n.at(5));
    p.skills = {{0x3c, si(n.at(6))}, {0x3d, si(n.at(7))}, {0x5c, si(n.at(8))}};
    return one(it::equip_restriction(t, p, item));
  }
  throw std::runtime_error("unknown operation " + op);
}
std::string invoke(const std::string &op, const std::vector<std::string> &a,
                   it::Rng &rng, bool &has_rng) {
  if (op == "scan") {
    const auto result = it::scan(string(a.at(0)), string(a.at(1)));
    Numbers out{static_cast<std::uint64_t>(result.result)};
    for (auto v : result.values)
      out.push_back(static_cast<std::uint64_t>(v));
    return nums(out);
  }
  if (op == "find_definition") {
    const auto t = tables();
    return one(
        it::find_definition(t, string(a.at(0)), numbers(a.at(1)).at(0) != 0)
            ->addr);
  }
  if (op == "parse_descriptor") {
    const auto t = tables();
    const auto n = numbers(a.at(1));
    has_rng = true;
    rng = it::Rng(u(n.at(2)));
    std::array<std::uint8_t, 72> stale_bytes{};
    for (unsigned k = 0; k < 72; ++k)
      stale_bytes[k] = static_cast<std::uint8_t>(k);
    return hex(it::parse_descriptor(t, string(a.at(0)), si(n.at(0)),
                                    n.at(1) != 0, &rng)
                   .pack(stale_bytes));
  }
  if (op == "short_record") {
    auto t = tables();
    return hex(it::Item::unpack(t, bytes(a.at(0))).pack());
  }
  return execute(op, a, rng, has_rng);
}
void anchors() {
  auto t = tables();
  auto i = it::parse_descriptor(t, "sample1+m00-32769", 0);
  require(i.get(0x23) == 32767, "modifier signed16 wrap");
  const auto scan = it::scan("word%d", "");
  require(scan.result == -1 && scan.values.empty(),
          "scanner EOF accepted match boundary");
  auto clone = i.copy();
  clone.count = 123;
  clone.a[0] = 7;
  require(it::same_identity(i, clone), "identity ignores mutable fields");
  clone.mods[0]->values[0] = 4;
  require(i.get(0x23) == 32767, "copied modifier ownership");
  it::Item slots(t.defs[0]);
  for (int k = 0; k < 4; ++k)
    require(slots.set_modifier(k, k) == k, "modifier insertion");
  require(slots.set_modifier(4, 5) == -1 && slots.get(4) == 0,
          "fifth modifier dropped");
  require(slots.set_modifier(2, 7) == 2 && slots.get(2) == 7,
          "existing modifier replacement");
  std::array<std::uint8_t, 72> stale_bytes{};
  stale_bytes.fill(0xaa);
  auto packed = slots.pack(stale_bytes);
  require(packed[6] == 0xaa && packed[0x22] == 0xaa && packed[0x44] == 0xaa,
          "stale bytes preserved");
  require(it::parse_descriptor(t, "not-present", 0).definition == t.defs[0],
          "unknown name first row");
  bool threw = false;
  try {
    it::float_times_double(r::ONE, 0x7ff0000000000000ull);
  } catch (const it::Unsupported &) {
    threw = true;
  }
  require(threw, "nonfinite double constant unsupported");
  threw = false;
  try {
    t.weapon_mult(-1);
  } catch (const it::Error &) {
    threw = true;
  }
  require(threw, "negative table index bounded");
  threw = false;
  try {
    it::parse_descriptor(t, "sample1");
  } catch (const it::Error &) {
    threw = true;
  }
  require(threw, "variant requires RNG");
}
} // namespace
int main(int argc, char **argv) {
  try {
    require(argc == 2 || argc == 3,
            "usage: items-tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream input(argv[1]);
    require(bool(input), "cannot open vector file");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      require(bool(trace), "cannot open trace");
    }
    std::string line;
    std::size_t lines = 0, rows = 0, errors = 0;
    std::map<std::string, std::size_t> counts;
    std::array<bool, 65536> signed16{};
    std::uint64_t digest = 14695981039346656037ull;
    while (std::getline(input, line)) {
      ++lines;
      if (lines == 1) {
        require(line == "# rta-items-v1", "wrong vector schema");
        continue;
      }
      require(!line.empty(), "empty vector record");
      if (line[0] == '#')
        continue;
      const auto fields = split(line, '\t');
      require(fields.size() == 5 && fields[0] == "items",
              "vector schema fields");
      const unsigned config = static_cast<unsigned>(rows % 16);
      const auto model = static_cast<r::Fpu::Model>(config / 4);
      const bool bias = (config & 2) != 0, nearest = (config & 1) != 0;
      auto &context = r::set_fpu(model, bias, nearest);
      it::Rng rng;
      bool has_rng = false;
      std::string result;
      try {
        result = invoke(fields[1], split(fields[2], '|'), rng, has_rng);
      } catch (const it::Unsupported &) {
        result = "unsupported";
        ++errors;
      } catch (const it::Error &) {
        result = "error";
        ++errors;
      }
      require(context.model == model && context.mul_bias == bias &&
                  context.div_nearest == nearest,
              "item call changed caller arithmetic configuration");
      const std::string state = has_rng ? nums({rng.seed, rng.draws}) : "-";
      if (result != fields[3] || state != fields[4]) {
        std::cerr << "Mismatch line " << lines << " " << fields[1]
                  << "\nexpected " << fields[3] << " state " << fields[4]
                  << "\nactual   " << result << " state " << state << '\n';
        return 1;
      }
      const std::string record = fields[0] + "\t" + fields[1] + "\t" +
                                 fields[2] + "\t" + result + "\t" + state +
                                 "\n";
      for (unsigned char c : record) {
        digest ^= c;
        digest *= 1099511628211ull;
      }
      if (trace.is_open())
        trace << record;
      ++counts[fields[1]];
      ++rows;
      if (fields[1] == "s16") {
        const auto n = numbers(fields[2]);
        require(n.size() == 1 && n[0] < 65536, "s16 exhaustive range");
        require(!signed16[n[0]], "duplicate s16 exhaustive value");
        signed16[n[0]] = true;
      }
    }
    require(input.eof() && lines > 2, "empty or failed vector read");
    require(rows == 113999 && counts.size() == 37,
            "missing or duplicate item vectors");
    require(
        std::all_of(signed16.begin(), signed16.end(), [](bool x) { return x; }),
        "incomplete signed16 coverage");
    if (trace.is_open()) {
      trace.flush();
      require(bool(trace), "trace write error");
    }
    std::cout << "items PASS: " << rows << " vectors, " << counts.size()
              << " operations, " << errors
              << " expected errors; fnv1a64=" << std::hex << std::setw(16)
              << std::setfill('0') << digest << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "items FAIL: " << e.what() << '\n';
    return 1;
  }
}
